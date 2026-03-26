#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import queue
import threading
import tkinter as tk
import sys
from pathlib import Path
from tkinter import filedialog, messagebox, ttk

try:
    from serial.tools import list_ports
except ModuleNotFoundError:
    _msg = (
        "缺少依赖 pyserial。\n\n"
        f"当前解释器:\n{sys.executable}\n\n"
        "请先执行:\n"
        f"\"{sys.executable}\" -m pip install pyserial"
    )
    try:
        _root = tk.Tk()
        _root.withdraw()
        messagebox.showerror("缺少依赖", _msg)
        _root.destroy()
    except Exception:
        pass
    print(_msg)
    raise SystemExit(1)

from OTA import run_ota


class OtaGui:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("JL701 UART OTA")
        self.root.geometry("760x560")
        self.root.minsize(720, 520)

        self.worker = None
        self.log_queue = queue.Queue()
        self.running = False

        self.port_var = tk.StringVar()
        self.baud_var = tk.StringVar(value="1000000")
        self.file_var = tk.StringVar()
        self.chunk_var = tk.StringVar(value="256")
        self.pkt_len_var = tk.StringVar(value="2048")
        self.timeout_var = tk.StringVar(value="8")
        self.crc_mode_var = tk.StringVar(value="zero")
        self.progress_text_var = tk.StringVar(value="未开始")

        self._build_ui()
        self._refresh_ports()
        self.root.after(100, self._drain_log_queue)

    def _build_ui(self):
        self.root.configure(bg="#e9edf2")

        top = tk.Frame(self.root, bg="#e9edf2")
        top.pack(fill="both", expand=True, padx=14, pady=14)

        form = tk.LabelFrame(top, text="OTA 参数", bg="#f7f9fc", padx=12, pady=10)
        form.pack(fill="x")

        self._add_row(form, 0, "串口", self._build_port_row)
        self._add_row(form, 1, "波特率", self._build_baud_row)
        self._add_row(form, 2, "固件文件", self._build_file_row)
        self._add_row(form, 3, "chunk", self._build_chunk_row)
        self._add_row(form, 4, "pkt_len", self._build_pkt_len_row)
        self._add_row(form, 5, "timeout", self._build_timeout_row)
        self._add_row(form, 6, "file_crc", self._build_crc_row)

        action = tk.Frame(top, bg="#e9edf2")
        action.pack(fill="x", pady=(12, 8))

        self.start_btn = tk.Button(
            action,
            text="开始 OTA",
            command=self._start_ota,
            bg="#1d6f42",
            fg="white",
            activebackground="#155532",
            relief="flat",
            width=14,
            height=2,
        )
        self.start_btn.pack(side="left")

        clear_btn = tk.Button(
            action,
            text="清空日志",
            command=self._clear_log,
            bg="#3b4a5a",
            fg="white",
            activebackground="#2c3946",
            relief="flat",
            width=12,
            height=2,
        )
        clear_btn.pack(side="left", padx=(10, 0))

        status = tk.LabelFrame(top, text="进度", bg="#f7f9fc", padx=12, pady=10)
        status.pack(fill="x", pady=(0, 8))

        self.progress = ttk.Progressbar(status, orient="horizontal", mode="determinate", maximum=100)
        self.progress.pack(fill="x")

        progress_label = tk.Label(status, textvariable=self.progress_text_var, anchor="w", bg="#f7f9fc")
        progress_label.pack(fill="x", pady=(8, 0))

        log_frame = tk.LabelFrame(top, text="日志", bg="#f7f9fc", padx=8, pady=8)
        log_frame.pack(fill="both", expand=True)

        self.log_text = tk.Text(
            log_frame,
            wrap="word",
            bg="#0f1720",
            fg="#d7e1ea",
            insertbackground="#d7e1ea",
            relief="flat",
        )
        self.log_text.pack(side="left", fill="both", expand=True)

        scroll = ttk.Scrollbar(log_frame, command=self.log_text.yview)
        scroll.pack(side="right", fill="y")
        self.log_text.configure(yscrollcommand=scroll.set)

    def _add_row(self, parent, row, label, builder):
        tk.Label(parent, text=label, width=10, anchor="w", bg="#f7f9fc").grid(row=row, column=0, sticky="w", pady=6)
        container = tk.Frame(parent, bg="#f7f9fc")
        container.grid(row=row, column=1, sticky="ew", pady=6)
        parent.grid_columnconfigure(1, weight=1)
        builder(container)

    def _build_port_row(self, parent):
        self.port_combo = ttk.Combobox(parent, textvariable=self.port_var, state="readonly")
        self.port_combo.pack(side="left", fill="x", expand=True)
        ttk.Button(parent, text="刷新", command=self._refresh_ports).pack(side="left", padx=(8, 0))

    def _build_baud_row(self, parent):
        ttk.Entry(parent, textvariable=self.baud_var).pack(fill="x")

    def _build_file_row(self, parent):
        ttk.Entry(parent, textvariable=self.file_var).pack(side="left", fill="x", expand=True)
        ttk.Button(parent, text="选择", command=self._select_file).pack(side="left", padx=(8, 0))

    def _build_chunk_row(self, parent):
        ttk.Entry(parent, textvariable=self.chunk_var).pack(fill="x")

    def _build_pkt_len_row(self, parent):
        ttk.Entry(parent, textvariable=self.pkt_len_var).pack(fill="x")

    def _build_timeout_row(self, parent):
        ttk.Entry(parent, textvariable=self.timeout_var).pack(fill="x")

    def _build_crc_row(self, parent):
        frame = tk.Frame(parent, bg="#f7f9fc")
        frame.pack(fill="x")
        ttk.Radiobutton(frame, text="zero", variable=self.crc_mode_var, value="zero").pack(side="left")
        ttk.Radiobutton(frame, text="crc16", variable=self.crc_mode_var, value="crc16").pack(side="left", padx=(12, 0))

    def _refresh_ports(self):
        ports = [p.device for p in list_ports.comports()]
        self.port_combo["values"] = ports
        if ports and self.port_var.get() not in ports:
            self.port_var.set(ports[0])

    def _select_file(self):
        path = filedialog.askopenfilename(
            title="选择 OTA 文件",
            filetypes=[("Binary", "*.bin"), ("All Files", "*.*")],
        )
        if path:
            self.file_var.set(path)

    def _clear_log(self):
        self.log_text.delete("1.0", tk.END)

    def _append_log(self, text: str):
        self.log_text.insert(tk.END, text + "\n")
        self.log_text.see(tk.END)

    def _drain_log_queue(self):
        while True:
            try:
                kind, payload = self.log_queue.get_nowait()
            except queue.Empty:
                break

            if kind == "log":
                self._append_log(payload)
            elif kind == "progress":
                sent, total = payload
                percent = 0 if total == 0 else sent * 100.0 / total
                self.progress["value"] = percent
                self.progress_text_var.set(f"{sent}/{total} bytes ({percent:.1f}%)")
            elif kind == "done":
                self.running = False
                self.start_btn.configure(state="normal")
                messagebox.showinfo("OTA", payload)
            elif kind == "error":
                self.running = False
                self.start_btn.configure(state="normal")
                messagebox.showerror("OTA", payload)
        self.root.after(100, self._drain_log_queue)

    def _validate(self):
        if self.running:
            raise ValueError("OTA 正在进行中")
        if not self.port_var.get().strip():
            raise ValueError("请选择串口")
        if not Path(self.file_var.get().strip()).is_file():
            raise ValueError("请选择有效的固件文件")
        int(self.baud_var.get().strip())
        int(self.chunk_var.get().strip())
        int(self.pkt_len_var.get().strip())
        float(self.timeout_var.get().strip())

    def _start_ota(self):
        try:
            self._validate()
        except Exception as e:
            messagebox.showerror("参数错误", str(e))
            return

        self.running = True
        self.progress["value"] = 0
        self.progress_text_var.set("准备开始")
        self.start_btn.configure(state="disabled")
        self._append_log("=" * 48)
        self._append_log("开始 OTA")

        kwargs = {
            "port": self.port_var.get().strip(),
            "baud": int(self.baud_var.get().strip()),
            "file_path": self.file_var.get().strip(),
            "chunk": int(self.chunk_var.get().strip()),
            "pkt_len": int(self.pkt_len_var.get().strip()),
            "timeout": float(self.timeout_var.get().strip()),
            "file_crc_mode": self.crc_mode_var.get().strip(),
        }

        self.worker = threading.Thread(target=self._worker_run, args=(kwargs,), daemon=True)
        self.worker.start()

    def _worker_run(self, kwargs):
        try:
            run_ota(
                **kwargs,
                progress_cb=lambda sent, total: self.log_queue.put(("progress", (sent, total))),
                log_cb=lambda msg: self.log_queue.put(("log", msg)),
            )
            self.log_queue.put(("done", "OTA 完成，设备应进入重启流程"))
        except Exception as e:
            self.log_queue.put(("error", f"OTA 失败: {e}"))


def main():
    root = tk.Tk()
    style = ttk.Style()
    try:
        style.theme_use("clam")
    except tk.TclError:
        pass
    OtaGui(root)
    root.mainloop()


if __name__ == "__main__":
    main()
