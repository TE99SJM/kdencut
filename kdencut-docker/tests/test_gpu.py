"""Host detection and in-container probes must have a usable CPU fallback."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
import test_media_server as common
import hardware

spec = importlib.util.spec_from_file_location('configure_gpu', Path(__file__).parents[1]/'configure-gpu.py')
configure = importlib.util.module_from_spec(spec); spec.loader.exec_module(configure)


class HardwareTests(unittest.TestCase):
    def test_no_gpu_or_missing_runtime_is_software(self):
        with tempfile.TemporaryDirectory() as root:
            for run in [lambda args:'', lambda args:'GPU-one' if args[0]=='nvidia-smi' else '{}']:
                result, mode = configure.configuration(dri=Path(root), run=run)
                self.assertEqual(mode, 'software')
                self.assertNotIn('devices', result['services']['kdencut'])
                self.assertEqual(hardware.detect(probe=lambda _:False)['backend'], 'software')

    def test_nvidia_runtime_and_hardware_are_both_required(self):
        def run(args): return 'GPU-one' if args[0]=='nvidia-smi' else '{"nvidia":{}}'
        with tempfile.TemporaryDirectory() as root:
            result, mode = configure.configuration(dri=Path(root), run=run)
        self.assertEqual(mode, 'nvidia')
        self.assertEqual(result['services']['kdencut']['deploy']['resources']['reservations']['devices'][0]['capabilities'], ['gpu'])

    def test_intel_or_amd_render_node_override_preserves_base_settings(self):
        with tempfile.TemporaryDirectory() as root:
            (Path(root)/'renderD129').touch()
            result, mode = configure.configuration(dri=Path(root), run=lambda _:'')
        self.assertEqual(mode, 'vaapi')
        service = result['services']['kdencut']
        self.assertEqual(service['environment']['DRINODE'], '/dev/dri/renderD129')
        for untouched in ['ports','volumes','build','container_name']: self.assertNotIn(untouched, service)
        self.assertTrue(set(service['environment']) <= {'KDENCUT_GPU','DRINODE','DRI_NODE'})

    def test_encoder_must_really_work_before_enabling_preview(self):
        calls = []
        def probe(args):
            calls.append(args)
            return 'h264_vaapi' in args and '/dev/dri/renderD129' in args
        info = hardware.detect(nodes=[Path('/dev/dri/renderD128'),Path('/dev/dri/renderD129')], probe=probe)
        self.assertEqual(info['encoder'], 'h264_vaapi')
        self.assertEqual(hardware.environment(info)['MLT_AVFORMAT_HWACCEL_DEVICE'], '/dev/dri/renderD129')
        self.assertEqual(len(calls), 3)
        self.assertEqual(hardware.detect('software', probe=lambda _:self.fail('CPU mode must not probe GPU'))['encoder'], 'libx264')

    def test_broken_driver_falls_back_and_clears_preview_acceleration(self):
        info = hardware.detect('nvidia', probe=lambda _:False)
        self.assertEqual(info['encoder'], 'libx264')
        self.assertEqual(hardware.environment(info)['MLT_AVFORMAT_HWACCEL'], '')
