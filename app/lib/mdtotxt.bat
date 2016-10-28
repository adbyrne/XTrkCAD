@echo off
REM Convert the Readme from Markdown to plain text
pandoc Readme.md --from Markdown --to plain -o Readme.txt