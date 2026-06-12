from pathlib import Path
import re
import textwrap


PAGE_W = 612
PAGE_H = 792
MARGIN_L = 58
MARGIN_R = 58
MARGIN_T = 70
MARGIN_B = 58
CONTENT_W = PAGE_W - MARGIN_L - MARGIN_R

BLUE = (18, 47, 89)
CYAN = (0, 156, 206)
LIGHT_CYAN = (229, 247, 252)
DARK = (31, 41, 55)
GRAY = (97, 111, 128)
LIGHT_GRAY = (242, 245, 248)
WHITE = (255, 255, 255)


DOC_META = {
    "USER_MANUAL": ("User Manual", "How to operate, calibrate, and troubleshoot the SmartGym prototype."),
    "DEVELOPER_GUIDE": ("Developer Guide", "Firmware architecture, build flow, modules, logs, and memory notes."),
    "FIREBASE_GUIDE": ("Firebase Guide", "Realtime Database paths, upload phases, and safe import guidance."),
    "SYSTEM_ARCHITECTURE": ("System Architecture", "Device, firmware, Firebase, and dashboard data flow."),
    "TROUBLESHOOTING": ("Troubleshooting Guide", "Common firmware, sync, UI, and dashboard issues."),
    "FIREBASE_WEBAPP_SCHEMA": ("Firebase Webapp Schema", "Dashboard-compatible Firebase session and summary structure."),
    "FIREBASE_DASHBOARD_SEED_GUIDE": ("Firebase Dashboard Seed Guide", "Seed data structure and safe dashboard test imports."),
    "MIGRATION_NOTES": ("Migration Notes", "Notes for schema, sync, and prototype migration work."),
    "SQUARELINE_HANDOFF": ("SquareLine Handoff", "UI handoff notes for SquareLine/LVGL assets."),
}


class PdfBuilder:
    def __init__(self, title, subtitle):
        self.title = title
        self.subtitle = subtitle
        self.objects = []
        self.pages = []
        self.current = []
        self.page_no = 0
        self.y = 0

    def _rgb(self, color):
        return " ".join(f"{c / 255:.4f}" for c in color)

    def _escape(self, text):
        return (
            text.encode("latin-1", "replace")
            .decode("latin-1")
            .replace("\\", "\\\\")
            .replace("(", "\\(")
            .replace(")", "\\)")
        )

    def _cmd(self, value):
        self.current.append(value)

    def add_obj(self, data):
        self.objects.append(data)
        return len(self.objects)

    def rect(self, x, y, w, h, fill, stroke=None):
        self._cmd(f"{self._rgb(fill)} rg")
        if stroke is None:
            self._cmd(f"{x:.1f} {y:.1f} {w:.1f} {h:.1f} re f")
        else:
            self._cmd(f"{self._rgb(stroke)} RG")
            self._cmd(f"{x:.1f} {y:.1f} {w:.1f} {h:.1f} re B")

    def line(self, x1, y1, x2, y2, color, width=1):
        self._cmd(f"{self._rgb(color)} RG")
        self._cmd(f"{width:.1f} w")
        self._cmd(f"{x1:.1f} {y1:.1f} m {x2:.1f} {y2:.1f} l S")

    def text(self, x, y, text, size=10, font="F1", color=DARK):
        self._cmd(f"{self._rgb(color)} rg")
        self._cmd("BT")
        self._cmd(f"/{font} {size:.1f} Tf")
        self._cmd(f"{x:.1f} {y:.1f} Td")
        self._cmd(f"({self._escape(text)}) Tj")
        self._cmd("ET")

    def text_width(self, text, size, font="F1"):
        factor = 0.54
        if font == "F2":
            factor = 0.58
        if font == "F3":
            factor = 0.60
        return len(text) * size * factor

    def wrap(self, text, size=10, width=CONTENT_W, font="F1"):
        avg = 0.54 if font != "F3" else 0.60
        chars = max(20, int(width / (size * avg)))
        return textwrap.wrap(text, width=chars, break_long_words=False, replace_whitespace=False) or [""]

    def cover(self):
        self.current = []
        self.page_no = 1
        self.rect(0, 0, PAGE_W, PAGE_H, WHITE)
        self.rect(0, PAGE_H - 185, PAGE_W, 185, BLUE)
        self.rect(0, PAGE_H - 190, PAGE_W, 8, CYAN)
        self.text(MARGIN_L, PAGE_H - 82, "SmartGym Adaptive Training System", 13, "F2", LIGHT_CYAN)
        self.text(MARGIN_L, PAGE_H - 128, self.title, 30, "F2", WHITE)
        for i, line in enumerate(self.wrap(self.subtitle, size=13, width=430, font="F1")):
            self.text(MARGIN_L, PAGE_H - 160 - (i * 18), line, 13, "F1", LIGHT_CYAN)
        self.text(MARGIN_L, 140, "Version 1.0 Prototype", 15, "F2", BLUE)
        self.text(MARGIN_L, 116, "ESP32-S3 firmware | Firebase RTDB | React dashboard", 11, "F1", GRAY)
        self.text(MARGIN_L, 92, "Generated documentation package", 10, "F1", GRAY)
        self.pages.append(self.current)
        self.new_page()

    def new_page(self):
        if self.current:
            self.pages.append(self.current)
        self.current = []
        self.page_no += 1
        self.rect(0, 0, PAGE_W, PAGE_H, WHITE)
        self.rect(0, PAGE_H - 42, PAGE_W, 42, BLUE)
        self.text(MARGIN_L, PAGE_H - 27, self.title, 10, "F2", WHITE)
        self.text(PAGE_W - MARGIN_R - 82, PAGE_H - 27, f"Page {self.page_no}", 9, "F1", LIGHT_CYAN)
        self.line(MARGIN_L, MARGIN_B - 18, PAGE_W - MARGIN_R, MARGIN_B - 18, LIGHT_GRAY, 1)
        self.text(MARGIN_L, MARGIN_B - 36, "SmartGym Adaptive Training System", 8, "F1", GRAY)
        self.y = PAGE_H - MARGIN_T

    def ensure(self, needed):
        if self.y - needed < MARGIN_B:
            self.new_page()

    def paragraph(self, text, size=10.5, leading=15, color=DARK, indent=0, font="F1", space_after=7):
        lines = self.wrap(text, size=size, width=CONTENT_W - indent, font=font)
        self.ensure(len(lines) * leading + space_after)
        for line in lines:
            self.text(MARGIN_L + indent, self.y, line, size, font, color)
            self.y -= leading
        self.y -= space_after

    def heading(self, text, level):
        if level == 1:
            self.ensure(44)
            self.y -= 8
            self.text(MARGIN_L, self.y, text, 19, "F2", BLUE)
            self.y -= 10
            self.line(MARGIN_L, self.y, MARGIN_L + 175, self.y, CYAN, 2)
            self.y -= 18
        elif level == 2:
            self.ensure(34)
            self.y -= 5
            self.text(MARGIN_L, self.y, text, 14, "F2", BLUE)
            self.y -= 21
        else:
            self.ensure(24)
            self.text(MARGIN_L, self.y, text, 11.5, "F2", DARK)
            self.y -= 18

    def bullet(self, text, ordered=None):
        self.ensure(26)
        label = f"{ordered}." if ordered is not None else "-"
        self.text(MARGIN_L + 10, self.y, label, 10.5, "F2", CYAN)
        lines = self.wrap(text, size=10.2, width=CONTENT_W - 36)
        for i, line in enumerate(lines):
            self.text(MARGIN_L + 34, self.y, line, 10.2, "F1", DARK)
            self.y -= 14
        self.y -= 4

    def code_block(self, lines):
        if not lines:
            return
        clean = [line.rstrip() for line in lines]
        wrapped = []
        for line in clean:
            wrapped.extend(textwrap.wrap(line, width=78, replace_whitespace=False) or [""])
        height = len(wrapped) * 12 + 18
        self.ensure(height + 10)
        self.rect(MARGIN_L, self.y - height + 6, CONTENT_W, height, LIGHT_GRAY, stroke=(220, 226, 233))
        y = self.y - 12
        for line in wrapped:
            self.text(MARGIN_L + 12, y, line, 8.6, "F3", DARK)
            y -= 12
        self.y -= height + 8

    def note_box(self, title, body):
        lines = self.wrap(body, size=10.2, width=CONTENT_W - 28)
        height = 34 + len(lines) * 14
        self.ensure(height + 10)
        self.rect(MARGIN_L, self.y - height + 8, CONTENT_W, height, LIGHT_CYAN, stroke=(173, 223, 236))
        self.text(MARGIN_L + 14, self.y - 12, title, 10.5, "F2", BLUE)
        y = self.y - 30
        for line in lines:
            self.text(MARGIN_L + 14, y, line, 10.0, "F1", DARK)
            y -= 14
        self.y -= height + 10

    def finish(self, out):
        if self.current:
            self.pages.append(self.current)
            self.current = []
        font_regular = self.add_obj(b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>")
        font_bold = self.add_obj(b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold >>")
        font_mono = self.add_obj(b"<< /Type /Font /Subtype /Type1 /BaseFont /Courier >>")
        page_ids = []
        content_ids = []
        for page in self.pages:
            stream = "\n".join(page).encode("latin-1", "replace")
            content_id = self.add_obj(b"<< /Length " + str(len(stream)).encode() + b" >>\nstream\n" + stream + b"\nendstream")
            content_ids.append(content_id)
            page_ids.append(self.add_obj(b""))
        kids = " ".join(f"{pid} 0 R" for pid in page_ids).encode()
        pages_id = self.add_obj(b"<< /Type /Pages /Kids [ " + kids + b" ] /Count " + str(len(page_ids)).encode() + b" >>")
        for idx, page_id in enumerate(page_ids):
            page_obj = (
                b"<< /Type /Page /Parent " + str(pages_id).encode() + b" 0 R /MediaBox [0 0 612 792] "
                b"/Resources << /Font << /F1 " + str(font_regular).encode() + b" 0 R /F2 "
                + str(font_bold).encode() + b" 0 R /F3 " + str(font_mono).encode()
                + b" 0 R >> >> /Contents " + str(content_ids[idx]).encode() + b" 0 R >>"
            )
            self.objects[page_id - 1] = page_obj
        catalog_id = self.add_obj(b"<< /Type /Catalog /Pages " + str(pages_id).encode() + b" 0 R >>")

        pdf = bytearray(b"%PDF-1.4\n")
        offsets = [0]
        for i, obj in enumerate(self.objects, start=1):
            offsets.append(len(pdf))
            pdf.extend(f"{i} 0 obj\n".encode())
            pdf.extend(obj)
            pdf.extend(b"\nendobj\n")
        xref = len(pdf)
        pdf.extend(f"xref\n0 {len(self.objects) + 1}\n".encode())
        pdf.extend(b"0000000000 65535 f\n")
        for off in offsets[1:]:
            pdf.extend(f"{off:010d} 00000 n\n".encode())
        pdf.extend(b"trailer\n")
        pdf.extend(b"<< /Size " + str(len(self.objects) + 1).encode() + b" /Root " + str(catalog_id).encode() + b" 0 R >>\n")
        pdf.extend(b"startxref\n" + str(xref).encode() + b"\n%%EOF\n")
        out.write_bytes(pdf)
        return len(self.pages)


def render_markdown(md_path: Path, out_path: Path):
    key = md_path.stem
    title, subtitle = DOC_META.get(key, (key.replace("_", " ").title(), "SmartGym project documentation."))
    pdf = PdfBuilder(title, subtitle)
    pdf.cover()

    in_code = False
    code_lines = []
    ordered_counter = 0
    pending_note = None

    for raw in md_path.read_text(encoding="utf-8").splitlines():
        line = raw.rstrip()
        if line.startswith("```"):
            if in_code:
                pdf.code_block(code_lines)
                code_lines = []
                in_code = False
            else:
                in_code = True
            continue
        if in_code:
            code_lines.append(line)
            continue
        if not line.strip():
            pdf.y -= 3
            continue
        m = re.match(r"^(#{1,4})\s+(.*)", line)
        if m:
            pdf.heading(m.group(2).strip(), min(len(m.group(1)), 3))
            ordered_counter = 0
            continue
        m = re.match(r"^\d+\.\s+(.*)", line)
        if m:
            ordered_counter += 1
            pdf.bullet(m.group(1).strip(), ordered_counter)
            continue
        if line.startswith("- "):
            ordered_counter = 0
            pdf.bullet(line[2:].strip())
            continue
        if line.startswith("> "):
            pdf.note_box("Note", line[2:].strip())
            continue
        if line.startswith("|"):
            pdf.code_block([line])
            continue
        ordered_counter = 0
        pdf.paragraph(line)

    if code_lines:
        pdf.code_block(code_lines)
    return pdf.finish(out_path)


def main():
    docs_dir = Path("docs")
    for md in sorted(docs_dir.glob("*.md")):
        out = md.with_suffix(".pdf")
        pages = render_markdown(md, out)
        print(f"{out} ({pages} pages, {out.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
