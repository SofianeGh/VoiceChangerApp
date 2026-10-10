"""Convertit un modèle de voix RVC (.pth, ou .zip contenant un .pth) en .onnx pour Voice Changer.

Usage : python convert_rvc.py <modele.pth|modele.zip> <sortie.onnx>

Nécessite torch, onnx, scipy et le code officiel RVC (version 2.2.231006) dans tools/rvc-src.
"""
import os
import sys
import tempfile
import zipfile

import torch

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(TOOLS_DIR, "rvc-src"))

from torch.nn import functional as F  # noqa: E402

from infer.lib.infer_pack import attentions, commons, models_onnx  # noqa: E402
from infer.lib.infer_pack.models_onnx import SynthesizerTrnMsNSFsidM  # noqa: E402


def _flow_forward(self, x, x_mask, g=None, reverse=False):
    """Corrige un défaut de la version 2.2.231006 : en sens inverse, chaque couche renvoie (x, logdet)
    alors que le bloc d'origine attend x seul, ce qui fait échouer l'export."""
    flows = reversed(self.flows) if reverse else self.flows
    for flow in flows:
        x, _ = flow(x, x_mask, g=g, reverse=reverse)
    return x


models_onnx.ResidualCouplingBlock.forward = _flow_forward


def _convert_pad_shape(pad_shape):
    """Remplace la version d'origine, qui passe par un tenseur : l'export y figeait la durée de l'exemple
    (200 trames) et le modèle exporté refusait ensuite toute autre durée."""
    return [item for pair in pad_shape[::-1] for item in pair]


commons.convert_pad_shape = _convert_pad_shape


# Même défaut dans l'attention relative : les int(length) de la version d'origine transforment la durée en
# constante pendant l'export. Ces deux versions gardent la durée dynamique.
def _relative_to_absolute(self, x):
    """x: [b, h, l, 2*l-1] -> [b, h, l, l]"""
    batch, heads, length, _ = x.size()
    x = F.pad(x, [0, 1, 0, 0, 0, 0, 0, 0])
    x_flat = x.view([batch, heads, length * 2 * length])
    x_flat = F.pad(x_flat, [0, length - 1, 0, 0, 0, 0])
    return x_flat.view([batch, heads, length + 1, 2 * length - 1])[:, :, :length, length - 1:]


def _absolute_to_relative(self, x):
    """x: [b, h, l, l] -> [b, h, l, 2*l-1]"""
    batch, heads, length, _ = x.size()
    x = F.pad(x, [0, length - 1, 0, 0, 0, 0, 0, 0])
    x_flat = x.view([batch, heads, length * length + length * (length - 1)])
    x_flat = F.pad(x_flat, [length, 0, 0, 0, 0, 0])
    return x_flat.view([batch, heads, length, 2 * length])[:, :, :, 1:]


attentions.MultiHeadAttention._relative_position_to_absolute_position = _relative_to_absolute
attentions.MultiHeadAttention._absolute_position_to_relative_position = _absolute_to_relative


def find_checkpoint(path, work_dir):
    """Renvoie le chemin du .pth, en l'extrayant de l'archive si besoin."""
    if not path.lower().endswith(".zip"):
        return path

    with zipfile.ZipFile(path) as archive:
        names = [n for n in archive.namelist() if n.lower().endswith(".pth")]
        if not names:
            raise SystemExit("Aucun fichier .pth dans l'archive : " + ", ".join(archive.namelist()))
        # Les checkpoints d'entraînement (G_xxx.pth, D_xxx.pth) ne sont pas des modèles utilisables
        names.sort(key=lambda n: os.path.basename(n).startswith(("G_", "D_")))
        target = os.path.join(work_dir, os.path.basename(names[0]))
        with archive.open(names[0]) as source, open(target, "wb") as out:
            out.write(source.read())
        return target


def convert(model_path, output_path):
    # weights_only=True : refuse d'exécuter du code caché dans le fichier (les .pth sont des pickles)
    cpt = torch.load(model_path, map_location="cpu", weights_only=True)
    if "weight" not in cpt or "config" not in cpt:
        raise SystemExit("Ce fichier n'est pas un modèle RVC utilisable (clés trouvées : %s)." % ", ".join(cpt.keys()))

    version = cpt.get("version", "v1")
    if cpt.get("f0", 1) != 1:
        raise SystemExit("Modèle sans hauteur de voix (f0 = 0) : non pris en charge.")

    cpt["config"][-3] = cpt["weight"]["emb_g.weight"].shape[0]
    vec_channels = 256 if version == "v1" else 768
    frames = 200

    net_g = SynthesizerTrnMsNSFsidM(*cpt["config"], is_half=False, version=version)
    net_g.load_state_dict(cpt["weight"], strict=False)
    net_g.eval()

    example = (
        torch.rand(1, frames, vec_channels),            # phone : vecteurs ContentVec
        torch.tensor([frames]).long(),                  # phone_lengths
        torch.randint(size=(1, frames), low=5, high=255),  # pitch : hauteur quantifiée
        torch.rand(1, frames),                          # pitchf : hauteur en Hz
        torch.LongTensor([0]),                          # ds : identifiant de voix
        torch.rand(1, 192, frames),                     # rnd : bruit
    )

    torch.onnx.export(
        net_g,
        example,
        output_path,
        dynamic_axes={"phone": [1], "pitch": [1], "pitchf": [1], "rnd": [2]},
        do_constant_folding=False,
        opset_version=13,
        verbose=False,
        input_names=["phone", "phone_lengths", "pitch", "pitchf", "ds", "rnd"],
        output_names=["audio"],
        dynamo=False,
    )

    sample_rate = cpt["config"][-1]
    print("Modele RVC %s, %s Hz, vecteurs de taille %d -> %s" % (version, sample_rate, vec_channels, output_path))


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    with tempfile.TemporaryDirectory() as work_dir:
        convert(find_checkpoint(sys.argv[1], work_dir), sys.argv[2])
