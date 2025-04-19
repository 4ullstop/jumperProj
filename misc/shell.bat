@echo off
call "D:\VisualStudio\VC\Auxiliary\Build\vcvarsall.bat" x64 8.1
set INCLUDE=C:\Program Files (x86)\Windows Kits\8.1\Include\shared;C:\Program Files (x86)\Windows Kits\8.1\Include\um;C:\Program Files (x86)\Windows Kits\8.1\Include\winrt;%INCLUDE%
set path=f:\misc;%path%
