#!/usr/bin/env bash
# ==============================================================================
# DEOS Firmware - Modular ICD Compilation Script
# Author: Emir Arkalı <emirarkali07@gmail.com>
# Purpose: Compiles all LaTeX ICD specifications in docs/icd/ to PDF,
#          executes double-pass for internal cross-references, and removes
#          all auxiliary LaTeX build files (*.aux, *.log, *.out, *.toc).
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "======================================================================"
echo " Starting DEOS ICD PDF Compilation..."
echo " Directory: $SCRIPT_DIR"
echo "======================================================================"

TEX_FILES=(
    "ICD_Core_Architecture.tex"
    "ICD_Data_Types_Config.tex"
    "ICD_Network_Management.tex"
    "ICD_Fault_Management.tex"
    "ICD_CANFD_Transport.tex"
    "ICD_Ethernet_Transport.tex"
    "ICD_LoRa_Transport.tex"
    "ICD_Traction.tex"
    "ICD_Steering.tex"
    "ICD_Brake.tex"
    "ICD_Lighting.tex"
    "ICD_BMS.tex"
)

SUCCESS_COUNT=0
FAIL_COUNT=0

for tex_file in "${TEX_FILES[@]}"; do
    if [[ ! -f "$tex_file" ]]; then
        echo "[WARN] File not found: $tex_file. Skipping."
        continue
    fi

    base_name="${tex_file%.tex}"
    echo -n "Compiling $tex_file (Pass 1/2 & 2/2)... "

    # Pass 1
    pdflatex -interaction=nonstopmode "$tex_file" > /dev/null 2>&1 || true
    # Pass 2 (resolves TOC and cross-references)
    if pdflatex -interaction=nonstopmode "$tex_file" > /dev/null 2>&1; then
        echo "[OK] -> ${base_name}.pdf"
        SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
    else
        echo "[FAIL]"
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi
done

echo "----------------------------------------------------------------------"
echo "Cleaning up auxiliary build artifacts (*.aux, *.log, *.out, *.toc, *.fls, *.fdb_latexmk)..."
rm -f *.aux *.log *.out *.toc *.fls *.fdb_latexmk *.synctex.gz

echo "======================================================================"
echo " Compilation Finished."
echo " Successfully compiled: $SUCCESS_COUNT / ${#TEX_FILES[@]}"
if [[ "$FAIL_COUNT" -gt 0 ]]; then
    echo " Failed: $FAIL_COUNT"
    exit 1
fi
echo " Only .tex and .pdf files remain in docs/icd/."
echo "======================================================================"
