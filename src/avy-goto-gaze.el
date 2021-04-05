(setq gaze-lib-file "/home/weitan/workspace/tobii-jump/build/libgaze.so")
(module-load gaze-lib-file)

(defun fake-module-reload (module)
  (interactive "fReload Module file: ")
  (let ((tmpfile (make-temp-file
                  (file-name-nondirectory module) nil module-file-suffix)))
    (copy-file module tmpfile t)
    (module-load tmpfile)))

(defun print-point (x y)
  (message "x: `%4f', y: `%4f'" x y)
  (not t))

(defun reset-gaze-vars ()
  (setq gaze-thread-should-exit nil)
  (setq gazed nil)
  (setq gazed-line nil))

(defun on-gaze-thread (col line)
  ;; (message "x: `%4f', y: `%4f'" x y)
  (goto-gaze col line)
  (setq gazed t)
  (thread-yield)
  gaze-thread-should-exit)

(defun on-not-change-thread (col line)
  (thread-yield)
  gaze-thread-should-exit)

(defun on-still-thread (col line)
  ;; FIXME: The avy would freeze the ui.
  ;; (avy-goto-word-0-gaze col line)
  (thread-yield)
  (or gazed
      gaze-thread-should-exit))

(defun on-still (col line)
  t)

(defun on-exit (col line)
  (message "exit with line: `%d', col: `%d'" line col)
  (avy-goto-word-0-gaze col line 1)
  t)

(defun goto-gaze-p-thread ()
  (let* ((size (max-lines-cols))
         (lines (car size))
         (cols (cdr size)))
    (gaze--use 'on-gaze-thread
               'on-not-change-thread
               'on-still-thread
               'on-exit
               nil
               cols
               lines
               200.0)))

(defun start-gaze-in-thread ()
  (interactive)
  (reset-gaze-vars)
  (make-thread 'goto-gaze-p-thread "gaze-thread"))

(defun avy-goto-gaze-internal ()
  (let* ((size (max-lines-cols))
         (lines (car size))
         (cols (cdr size)))
    (gaze--use nil
               nil
               'on-still
               'on-exit
               t
               cols
               lines
               50.0)))

(defun avy-goto-gaze-optimized-internal ()
  (let* ((size (max-lines-cols))
         (lines (car size))
         (cols (cdr size)))
    (gaze--subscribe nil
               nil
               'on-still
               'on-exit
               t
               cols
               lines
               50.0)))

(defun gaze-init ()
  (interactive)
  (reset-gaze-vars)
  (gaze--init))

(defun gaze-cleanup ()
  (interactive)
  (gaze--clenaup))

(defun avy-goto-gaze ()
  (interactive)
  (reset-gaze-vars)
  (avy-goto-gaze-internal))

(defun avy-goto-gaze-optimized ()
  (interactive)
  (reset-gaze-vars)
  (avy-goto-gaze-optimized-internal))

(defun stop-gaze-in-thread ()
  (interactive)
  (setq gaze-thread-should-exit t))

(defun max-lines-cols ()
  (let* ((w-edges (window-edges nil nil nil nil))
         (cols (nth 2 w-edges))
         (lines (nth 3 w-edges)) )
    (cons lines cols)))

(defun point-of-position (line col)
  (car (compute-motion (window-start)
                       '(0 . 0)
                       (point-max)
                       (cons col line)
                       (window-width)
                       (cons (window-hscroll) 0)
                       (selected-window))))

(defun percent-to-point (x y)
  (let* ((size (max-lines-cols))
         (lines (car size))
         (cols (cdr size)))
    (cons (ceiling (* lines y)) (ceiling (* cols x)))))

(defun goto-gaze (col line)
  (let* ((first-line (line-number-at-pos (window-start)))
         (abs-line (+ first-line line)))
    ;; (message "line: `%d', col: `%d'" line col)
    ;; (goto-char (point-of-position line col))
    (setq gazed-line abs-line)
    (goto-char (point-min))
    (forward-line (1- abs-line))
    ;; (forward-char col)
    )
  )

(defun point-range-around-position (line col line-delta)
  (let* ((start (cons (- line line-delta) (- col 10)))
         (end (cons (+ line line-delta) (+ col 10))))
    (cons (point-of-position (car start) (cdr start))
          (point-of-position (car end) (cdr end)))))

(defun avy-goto-word-0-gaze (col line line-delta)
  (let ((range (point-range-around-position line col line-delta)))
    (avy-goto-word-0 nil (car range) (cdr range))))

(provide 'avy-goto-gaze)
