set TARGETS=old.txt new-ymm.txt bf16.txt misc.txt convert.txt minmax.txt saturation.txt
for %%f in (%TARGETS%) do (
  echo %%f
  call test_by_xed.bat avx10\%%f
)