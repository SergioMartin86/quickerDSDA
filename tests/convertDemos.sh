
fileList=`ls *.lmp`
for f in ${fileList}; do
 dest="${f%.lmp}.sol"
 echo "Converting: ${f} -> ${dest}"
 ../build/lmpConverter ${f} > ${dest}
done
