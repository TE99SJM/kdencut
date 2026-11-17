"""Keep phone integration isolated when merging new upstream MainWindow code."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]


class TouchIntegration(unittest.TestCase):
    def test_phone_methods_have_one_separate_implementation(self):
        main = (ROOT / 'src/mainwindow.cpp').read_text()
        touch = (ROOT / 'src/kdencut/mainwindowtouch.cpp').read_text()
        header = (ROOT / 'src/mainwindow.h').read_text()
        cmake = (ROOT / 'src/CMakeLists.txt').read_text()
        self.assertEqual(cmake.count('kdencut/mainwindowtouch.cpp'), 1)
        for name in ('initializeTouchWorkspace', 'connectTouchDock', 'updateTouchLayout',
                     'leaveTouchLayout', 'showTouchPanel', 'closeTouchPanel',
                     'touchCommand', 'importTouchMedia', 'layoutForSaving'):
            self.assertEqual(len(re.findall(r'MainWindow::' + name + r'\([^;]*?\)\s*(?:const\s*)?\{', touch)), 1, name)
            self.assertNotRegex(main, r'(?:void|QByteArray) MainWindow::' + name + r'\(')
            self.assertIn(name + '(', header)
        # MainWindow must still connect lifecycle and per-timeline entry points.
        for entry in ('initializeTouchWorkspace();', 'connectTouchDock(dock);',
                      '&TimelineWidget::touchCommand', 'leaveTouchLayout();'):
            self.assertIn(entry, main)
        self.assertIn('trackMediaProjectUsage(this)', touch)


if __name__ == '__main__': unittest.main()
