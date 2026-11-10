"""Headless layout and selection checks for the actual Splash.qml (requires PySide6).

Only KDE translation and qrc image paths are substituted in the test copy.
"""
import os
import re
import argparse
os.environ['QT_QPA_PLATFORM']='offscreen'
os.environ['QT_QUICK_BACKEND']='software'
from pathlib import Path
from PySide6.QtCore import QUrl, QPointF, QMetaObject
from PySide6.QtGui import QGuiApplication, QFont
from PySide6.QtQml import QQmlApplicationEngine
from PySide6.QtQuick import QQuickItem
os.chdir(Path(__file__).resolve().parents[2])
parser = argparse.ArgumentParser()
parser.add_argument('--font-size', type=int, default=10)
args = parser.parse_args()
app=QGuiApplication([])
app.setFont(QFont('Sans Serif', args.font_size))
source=Path('src/dialogs/Splash.qml').read_text().replace('import org.kde.ki18n','')
source=source.replace('    id: splash','''    id: splash
    property QtObject translator: QtObject {
        function i18n(text, a, b) { return text.replace('%1', a || '').replace('%2', b || '') }
    }''',1).replace('KI18n.i18n','splash.translator.i18n')
source=re.sub(r'objectName: \"[^\"]*\"', '', source)
source=re.sub(r'id: (\w+)',r'id: \1; objectName: "\1"',source)
source=source.replace('qrc:/pics/', QUrl.fromLocalFile(str(Path('data/pics').resolve())).toString()+'/')
engine=QQmlApplicationEngine()
engine.loadData(source.encode(),QUrl('file:///tmp/Splash.qml'))
assert engine.rootObjects(), 'QML failed to load'
root=engine.rootObjects()[0]
root.setProperty('actionsEnabled',True)
root.setProperty('touchEnabled',False)
for w,h in [(320,568),(375,667),(430,850),(932,430),(960,1700),(1280,720)]:
 root.setWidth(w);root.setHeight(h)
 for _ in range(10): app.processEvents()
 for name in ['themeSwitch','horizontalFrame','verticalFrame','profileCombo','fpsCombo','vTracks','aTracks','buttonHelp','buttonNext']:
  item=root.findChild(QQuickItem,name)
  pos=item.mapToScene(QPointF(0,0))
  assert pos.x()>=-0.1 and pos.x()+item.width()<=w+.1,(w,h,name,pos,item.width())
  if name.startswith('button'):
   assert 0<=pos.y() and pos.y()+item.height()<=h+.1,(w,h,name,pos,item.height())

 print('PASS setup controls fit',w,h)

# Changing orientation/resolution/FPS still emits the selected project values.
root.findChild(QQuickItem,'horizontalFrame').setProperty('checked',False)
root.findChild(QQuickItem,'verticalFrame').setProperty('checked',True)
for _ in range(5): app.processEvents()
root.findChild(QQuickItem,'profileCombo').setProperty('currentIndex',0)
root.findChild(QQuickItem,'fpsCombo').setProperty('currentIndex',2)
root.findChild(QQuickItem,'vTracks').setProperty('value',3)
root.findChild(QQuickItem,'aTracks').setProperty('value',1)
selected=[]
root.firstStart.connect(lambda *args: selected.append(args))
QMetaObject.invokeMethod(root.findChild(QQuickItem,'buttonNext'),'clicked')
assert selected == [('1080x1920x709x1x1x9x16','60',False,3,1)], selected
print('PASS project setup selections')

# The phone home is the real QML component, including its project actions.
root.setProperty('touchEnabled', True)
root.setProperty('firstRun', False)
root.setProperty('fileNames', ['A very long saved project name for a small phone.kdenlive'])
root.setProperty('fileDates', ['10/11/26'])
root.setProperty('urls', ['/media/very-long-project-name.kdenlive'])
root.show()
for w,h in [(320,568),(375,667),(430,850),(932,430)]:
 root.setWidth(w);root.setHeight(h)
 for _ in range(10): app.processEvents()
 for name in ['mobileNewVideo','mobileOrientation','mobileOpenProject']:
  item=root.findChild(QQuickItem,name)
  pos=item.mapToScene(QPointF(0,0))
  assert item.isVisible(),name
  assert pos.x()>=0 and pos.x()+item.width()<=w+.1,(w,h,name,pos,item.width())
 print('PASS phone home controls fit',w,h)
root.findChild(QQuickItem,'mobileOrientation').setProperty('currentIndex',1)
root.setProperty('mobileBusy',False)
QMetaObject.invokeMethod(root.findChild(QQuickItem,'mobileNewVideo'),'clicked')
assert selected[-1] == ('1920x1080x709x1x1x16x9','30',False,2,2),selected
print('PASS phone New video uses selected orientation')
