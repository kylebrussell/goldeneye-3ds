#!/usr/bin/env python3
"""Extract exact first-person resources from the verified US ROM."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import sys
import zlib


ROM_SHA1 = "abe01e4aeb033b6c0836819f549c791b26cfde83"


@dataclass(frozen=True)
class Resource:
    name: str
    item: str
    rom_start: int
    compressed_size: int
    compressed_sha256: str
    model_size: int
    model_sha256: str


RESOURCES = (
    Resource("GwppkZ", "ITEM_WPPK", 0x7A6FB0, 7312,
             "3098ca1feba182fb761844105a6d820691da65557182a2c0d67d6d03cf93449b",
             18512,
             "a209c8e63edc5cf35a8e938f6e880fe426754e6bb0f01ac58e7b33caa6f5815e"),
    Resource("GwppksilZ", "ITEM_WPPKSIL", 0x7A8C40, 7488,
             "f472d4e5ba4111abde49b4f7f889fc774f614cbb2a4c19d1bba55ddc2f5e457f",
             19536,
             "72090e104b8d360503d90958d708522f6a6e737a41859d01577fdc8f222d4d6c"),
    Resource("GbugZ", "ITEM_BUG", 0x770130, 2416,
             "c33fa6b09c5322da1bcdaf6cbdc7d5133aad3d93576e430dee90dd69ded7a92b",
             6848,
             "ce81d5c0081afe3668995edda908224a9f718dccef57ee7e525cacc7da4d927d"),
    Resource("Gak47Z", "ITEM_AK47", 0x76BF70, 2576,
             "d6aabedc315406bd74e91c6975423f8be119f49cf3c21dd5da2a470740ae296c",
             8240,
             "d2fce821439cf81175778a9e31b985fc0e5a76f864eda77a742654807366fa6c"),
    Resource("GremotemineZ", "ITEM_REMOTEMINE", 0x788F90, 2496,
             "435620ea5324cc586da86d5da9ac6b6714228b4e9309f7a68fdcf8690a9f8c6a",
             6512,
             "00cb29abf5e5663a954f1b0b289cf2d491fc4e855a5f9bcd7ef104e46d86d669"),
    Resource("GsniperrifleZ", "ITEM_SNIPERRIFLE", 0x790AD0, 4208,
             "6b4c13df3aab59f91ffed03bb2f7a14d584342c666b0efc5d21122ead2b074c0",
             13040,
             "ef7aecf4d9fedcbd2af6e44c960bf35a5936d00ad3c540f045c97223e48617c5"),
    Resource("GtriggerZ", "ITEM_TRIGGER", 0x797410, 13312,
             "398783785262e9df26e70a72f14249daeb6175642009a4d299dc20acadc540e0",
             35936,
             "054645203e88cd17ae882607a6ca5ef5fa963b49d6ddbbabf2d43ec945605a32"),
    Resource("GfistZ", "ITEM_FIST", 0x774A40, 5888,
             "56781fd5686ad450da472aedf22d327a2468da170eee82079f91b2fc76bf56c2",
             15072,
             "2eef18510058143d37440d8625b4e2dd32ce2d98f6d9a6b3f08615f82c8d5be9"),
    Resource("Gmp5ksilZ", "ITEM_MP5KSIL", 0x786CD0, 3328,
             "aace55c3d0eb0f4c27bb0a829e93d3d64fb7b7aa196ebb0508cd727b6287cbf3",
             10336,
             "3e79c567661f2178ce43cc9fe98f984c0199cad6014c2ce1cc5b2c481a1870c2"),
    Resource("GplastiqueZ", "ITEM_PLASTIQUE", 0x787EF0, 848,
             "c20efcc0f1a547c3194a6dc3436e1b69af3b800010a8afb4c8b8f50eb854d2a6",
             2336,
             "b88c1372e6f0b78f3b7b63ca0afa69db3ae188ec79324ffd71a997b0fd56c901"),
    Resource("GcameraZ", "ITEM_CAMERA", 0x771140, 1152,
             "51f4fc28f794bb4f188844b77696a737a4e8d42ef03ae46e94f8b51ecf917285",
             3232,
             "cc303629ac4ea2e487f46b106002897a15f4aa45b9c43edc5a028303442924cd"),
    Resource("GwatchmagnetattractZ", "ITEM_WATCHMAGNETATTRACT", 0x7A3F70,
             5200,
             "32524e6607efe2e95f4cf4598473eae8f0189ec4146f0f2786a5f1685001f6f9",
             15312,
             "32365d2a89a2ed2db6626997fbd24daaf2968e877b1461a074d484b08a24c490"),
    Resource("GuziZ", "ITEM_UZI", 0x79C330, 2320,
             "73601b23b60d396afed043153015cce0ba48ebf9d514b57f78ed024e1bef2049",
             7504,
             "28e00a6aee0bb8cf3f809c88f94624f6ebe6b1d5b456d373b7e68dd941c33e0b"),
    Resource("GwatchlaserZ", "ITEM_WATCHLASER", 0x7A0B70, 13312,
             "398783785262e9df26e70a72f14249daeb6175642009a4d299dc20acadc540e0",
             35936,
             "054645203e88cd17ae882607a6ca5ef5fa963b49d6ddbbabf2d43ec945605a32"),
    Resource("GgrenadelaunchZ", "ITEM_GRENADELAUNCH", 0x77C3D0, 4224,
             "4b8a6a42a68fb84fbaeb0aec71e81af29b66dc998eefd7916f1f47c0aa4137b8",
             13520,
             "0d2da8d133525ae0618e1cdffc72751e6da6fb0552c45a168ba4a7b06122033b"),
    Resource("GgrenadeZ", "ITEM_GRENADE", 0x77B9A0, 2608,
             "2e6931802a0abbbb3e2bf58e9b7b6f99977a4a26ffecd89ad01bfb9c09c23963",
             7840,
             "00d9353d6262b6b4ad8be82b7d4416cf42899d673d35b69c630c157f269d0d1b"),
    Resource("GtimedmineZ", "ITEM_TIMEDMINE", 0x796950, 2752,
             "3dcd332c0899836ccb6298d717649f34082b75e4503649fdca83039f3cf71b30",
             7136,
             "f603c8c48f15e391ba2143d80b71b58aa4296d5e9b535f820d986ad07fb23696"),
    Resource("GbombcaseZ", "ITEM_BOMBCASE", 0x76EC20, 1936,
             "d0bc6c4b988a240449e882eecf293476e06fffb9d67c24a2faecc1b8edbadf23",
             6272,
             "fdb64b6801a5ecb21b25a128160f26f22dd5a414dd1abb4c133d20cedfdfa1e1"),
    Resource("GmicrocameraZ", "ITEM_MICROCAMERA", 0x7850C0, 1600,
             "90912a065f9efd9a366118f373ba025dc7cc8203eff8b22faee04b53087a1ffd",
             4016,
             "5deec6c27dde6c405b49855541a1f705edaafe960f57744a6a246a0465ce5748"),
    Resource("GgoldeneyekeyZ", "ITEM_GOLDENEYEKEY", 0x777EB0, 2480,
             "f75e442b6805e397eef46ae00b857302759b423d1b2fa2bd87398fe908994803",
             7648,
             "afb8aee76252cd692ca2e8b7dfffcaea892de191896d1a5907bc41d15cf6a7b1"),
    Resource("Gfnp90Z", "ITEM_FNP90", 0x776490, 3232,
             "3ecbaae64b8b542eadad7cd9ea9bfdaca2574f62e451dead5de43647f0e06a47",
             9552,
             "7d1b2c3e220d11c7e3abac41a51ffd70bd5660c4751be2abd607ef36497f2168"),
    Resource("GrugerZ", "ITEM_RUGER", 0x78AB70, 7568,
             "0d914ce7103bb4a29ca64dae46741b663d1fa61e3864d6267b1e2deb3b34e1c6",
             19376,
             "d51288d2aa2d56b77e7205bce0981351a20561429e61543a8e6cb9a7e8fde6f2"),
    Resource("GspectreZ", "ITEM_SPECTRE", 0x791B40, 3200,
             "32192de7d75a077c1cac94e79a4c5a8736ec1cab193dfce2b427b528080701a8",
             9824,
             "d264dcd35efd8b716e96546492801e6d831037606e5c3bf24d92319053fdb391"),
    Resource("Gm16Z", "ITEM_M16", 0x7845B0, 2592,
             "16ba44e0a9f3176d75fea43e7435793506ca74128ebdd10bf9501bfd5d5db443",
             7472,
             "b7aef425df2373401579520774a74a72a6d98f231502f9ceabeb0471b5b7df24"),
    Resource("GshotgunZ", "ITEM_SHOTGUN", 0x78D090, 3808,
             "587018dec9110b9e13610b82bdf1e55e65cef962afb0e9179fbea82d309ac18a",
             12112,
             "98dd2141dd3777c0f19702906ab9b682da404a942f9cc1701e188c2741e9546f"),
    Resource("GautoshotZ", "ITEM_AUTOSHOT", 0x76CD10, 6160,
             "209f27aff3b7562d7150858d0811772688a712c60056496e9a38c68ebf487f54",
             20016,
             "8f8bc2a543d8e5b7436c0c31766975464672b8881ee11d397fcea7947ed2a082"),
    Resource("Gmp5kZ", "ITEM_MP5K", 0x7860F0, 3040,
             "6f8616ec6b0ba6bfc2950631b38587b8f1f8f7c52a9916b892fd024d32285379",
             9504,
             "b6c77b9279f4031eca19f0dc5beb9e0e4078e3655b86e85a56ad617e11c24555"),
    Resource("Gtt33Z", "ITEM_TT33", 0x79A810, 6944,
             "930c9cb794047aab202ce1b1af60e32107251ad93cfecec7734d658e66359f60",
             17584,
             "5b8b500a55018d99a9747c783c1ee6b42054a1b10c2732734d3fca23f0ad3db0"),
    Resource("GskorpionZ", "ITEM_SKORPION", 0x78F8D0, 4608,
             "1448261d2a30f9864e063a8043b24568e51284e70901f91bcb1ad6b9b91ca1a4",
             13536,
             "012f06d5cc0f8cf700f8d20998328193012c177056ad5c66b3873d24a5747ffd"),
    Resource("GknifeZ", "ITEM_KNIFE", 0x781650, 6864,
             "7cfabbbcc0c4a3a3c64fd7faf7468e74d6364c29655634ff6407c92973b01a11",
             18416,
             "b8360d0132d7f184fbfb30cc4485d49c96db1f5ebeac3c0b9e0e1da934069920"),
    Resource("GthrowknifeZ", "ITEM_THROWKNIFE", 0x794E60, 6896,
             "a2bbf8ba9f50fb6cdd5c5973793aa7f8d7f1ae276baa08343c7d8184f6394c9c",
             18240,
             "0f1d3ac177abbb4ef7e11ff961c4389b0625f2c4475ba263c91744d5b631eac8"),
    Resource("GgoldengunZ", "ITEM_GOLDENGUN", 0x778860, 6112,
             "416c06a6fee84d5194ce5fb58ca2a0ecaebf8ae1902e45a8806181782e8713cb",
             16320,
             "9250cf73f4e501e9fb1bc1db9fd0934f9a590f40a471c70a395af2e45ac0b6f2"),
    Resource("GsilverwppkZ", "ITEM_SILVERWPPK", 0x78DF70, 6496,
             "c3f422299f2ff4c3e036253fddf8f53e8b3c19d105e6b3ed25eeaeb8bdcdfd23",
             16480,
             "3c9c66924c7d6f65152b7ac42822438b43bca69a41fd2df21dcc45748c0d3bd1"),
    Resource("GgoldwppkZ", "ITEM_GOLDWPPK", 0x77A040, 6496,
             "7ea891609c183d13b3ac141efb6ffcb054222b16684d55836656f20a93356caa",
             16480,
             "32540c86e334ce156ddeb1515e0ea05875273d4d30dd7f21e53dd6d64c925f55"),
    Resource("GlaserZ", "ITEM_LASER", 0x783120, 3568,
             "d4dbedf442e38606dca0aad22015ff51bc6bc740eae3fbe443eab4228bea4202",
             10512,
             "585ec15c075bf238d16a16527f35109e1712c3d9ce79eddc7f0f15b560d5b51c"),
    Resource("GrocketlaunchZ", "ITEM_ROCKETLAUNCH", 0x789950, 4640,
             "94bae02b5a806c40d2c9991b8112a67399b517ba5383a395e26392e4eaa1f2df",
             12704,
             "c10ed15df85dfc7db9f6bf27b93556e9b751b2c42bf2d0e5109cee76eacded61"),
    Resource("GproximitymineZ", "ITEM_PROXIMITYMINE", 0x7887A0, 2032,
             "2d3f8d16b9d2f9d401738632605a1111321cf15ede88eaa3c1cdd6e44bcacb1a",
             5360,
             "81fe1a02c52faa2dae2459c74893e26de6718b30a56a912c783581af3cce58ed"),
    Resource("GtaserZ", "ITEM_TASER", 0x793000, 7776,
             "8a6b26a3c6b830e7bd0efd6b248769dcdb4a398b846bdbeadfaa8865450e7e1c",
             19984,
             "edcde38280d3bc07571cbf6cf22b4712712ccca1005c6784ae3f3b2d00963136"),
    Resource("GflarepistolZ", "ITEM_FLAREPISTOL", 0x776140, 848,
             "6fd8a19d6bfe89528afe6434468b9eb2855edbe308ea6d5429aedb3f83e7bc27",
             1952,
             "c1a40482cce5d3d6c4815ff5345c7a563a764ed5bd242b7ad4db14fca716090f"),
    Resource("GpitongunZ", "ITEM_PITONGUN", 0x7879D0, 848,
             "6fd8a19d6bfe89528afe6434468b9eb2855edbe308ea6d5429aedb3f83e7bc27",
             1952,
             "c1a40482cce5d3d6c4815ff5345c7a563a764ed5bd242b7ad4db14fca716090f"),
    Resource("Csuit_lf_handZ", "ITEM_SUIT_LF_HAND", 0x7524B0, 12832,
             "5050d08f74e4c094190b684da69635cc45afc6f032fe43158e79da6d4b47aebe",
             38688,
             "a89741cef49e4d4bf20368bfc5a521fcf196f789e084b4cf49add7c4da73dfd7"),
    Resource("GjoypadZ", "ITEM_JOYPAD", 0x77D7A0, 7856,
             "d32c8bbd9c2759b83b75e981ae8ee6b47d9f045a1ac9a966479f634899028528",
             21008,
             "b07b4bcfb45ca40f5e1816b545ba67f55b1d362b898c497a2cb94dba97eb55ae"),
)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def inflate_1172(data: bytes) -> bytes:
    if data[:2] != b"\x11\x72":
        raise ValueError("resource is missing Rare 1172 header")
    inflater = zlib.decompressobj(-15)
    result = inflater.decompress(data[2:]) + inflater.flush()
    if not inflater.eof or inflater.unused_data.strip(b"\0"):
        raise ValueError("invalid Rare 1172 deflate stream")
    return result


def extract(rom_path: Path, output: Path) -> dict[str, object]:
    rom = rom_path.read_bytes()
    if hashlib.sha1(rom).hexdigest() != ROM_SHA1:
        raise ValueError("unsupported ROM; expected unmodified US big-endian release")
    manifest_resources = []
    blobs: list[tuple[str, bytes]] = []
    for resource in RESOURCES:
        compressed = rom[
            resource.rom_start:resource.rom_start + resource.compressed_size]
        if (len(compressed) != resource.compressed_size
                or sha256(compressed) != resource.compressed_sha256):
            raise ValueError(f"{resource.name} compressed resource hash mismatch")
        model = inflate_1172(compressed)
        if (len(model) != resource.model_size
                or sha256(model) != resource.model_sha256):
            raise ValueError(f"{resource.name} model hash mismatch")
        blobs.append((resource.name, model))
        manifest_resources.append({
            "name": resource.name,
            "item": resource.item,
            "rom_start": resource.rom_start,
            "compressed_size": resource.compressed_size,
            "compressed_sha256": resource.compressed_sha256,
            "model_size": resource.model_size,
            "model_sha256": resource.model_sha256,
            "segmented_base": 0x05000000,
        })
    output.mkdir(parents=True, exist_ok=True)
    for name, model in blobs:
        (output / f"{name}.bin").write_bytes(model)
    manifest = {
        "schema": 1,
        "source_rom_sha1": ROM_SHA1,
        "resources": manifest_resources,
    }
    (output / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n")
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    try:
        manifest = extract(args.rom, args.output)
    except (OSError, ValueError, zlib.error) as error:
        raise SystemExit(f"cannot extract first-person models: {error}") from error
    print(f"extracted {len(manifest['resources'])} first-person models -> {args.output.absolute()}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
