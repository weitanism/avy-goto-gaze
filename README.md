avy-goto-gaze
---

使用 [Tobii](https://www.tobii.com/) 眼动仪配合 [avy](https://github.com/abo-abo/avy) 来跳转光标。

流程：眼动仪提供眼睛注视的屏幕位置 => 屏幕位置转换为 Emacs buffer 内的一个区域 => 调用 avy
