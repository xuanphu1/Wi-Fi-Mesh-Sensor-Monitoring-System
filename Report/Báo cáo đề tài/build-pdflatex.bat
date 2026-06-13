@echo off
setlocal EnableExtensions
pushd "%~dp0"

echo Cleaning LaTeX auxiliary files...
if exist main.aux del /Q "main.aux"
if exist main.log del /Q "main.log"
if exist main.toc del /Q "main.toc"
if exist main.lof del /Q "main.lof"
if exist main.lot del /Q "main.lot"
if exist main.out del /Q "main.out"
if exist main.bbl del /Q "main.bbl"
if exist main.blg del /Q "main.blg"
if exist main.synctex.gz del /Q "main.synctex.gz"
echo Done cleaning.
echo.

chcp 65001 >nul 2>&1
echo Building LaTeX document (pass 1)...
pdflatex -synctex=1 -interaction=nonstopmode -file-line-error main.tex
echo.
echo Building LaTeX document (pass 2 - TOC and refs)...
pdflatex -synctex=1 -interaction=nonstopmode -file-line-error main.tex
echo.
echo Done!
popd
endlocal
