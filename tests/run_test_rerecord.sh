#!/bin/bash

# Stop if anything fails
set -e

# Getting executable paths
baseExecutable=${1}
newExecutable=${2}

# Getting script name
script=${3}

# Getting additional arguments
testerArgs=${@:4}

# Getting current folder (game name)
folder=`basename $PWD`

# Getting pid (for uniqueness)
pid=$$

# Hash files
baseHashFile="/tmp/baseDSDA.${folder}.${script}.${pid}.hash"
newHashFile="/tmp/newDSDA.${folder}.${script}.${pid}.hash"

# Removing them if already present
rm -f ${baseHashFile}
rm -f ${newHashFile}.simple
rm -f ${newHashFile}.rerecord

set -x
# Running script on base DSDA
${baseExecutable} ${script} --hashOutputFile ${baseHashFile}.simple ${testerArgs} --cycleType Simple

# Running script on new DSDA (Simple)
${newExecutable} ${script} --hashOutputFile ${newHashFile}.simple ${testerArgs} --cycleType Simple

# Running script on base DSDA (Rerecord)
${newExecutable} ${script} --hashOutputFile ${baseHashFile}.rerecord ${testerArgs} --cycleType Rerecord --rerecordDepth 16

# Running script on new DSDA (Rerecord)
${newExecutable} ${script} --hashOutputFile ${newHashFile}.rerecord ${testerArgs} --cycleType Rerecord --rerecordDepth 16
set +x

# Comparing hashes
baseHash=`cat ${baseHashFile}.simple`
newHashSimple=`cat ${newHashFile}.simple`
baseHashRerecord=`cat ${newHashFile}.rerecord`
newHashRerecord=`cat ${newHashFile}.rerecord`

# Removing temporary files
rm -f ${baseHashFile}.simple ${newHashFile}.simple ${newHashFile}.rerecord

# Compare hashes (Simple)
if [ "${baseHash}" = "${newHashSimple}" ]; then
 echo "[] Simple Test Passed"
 exit 0
else
 echo "[] Simple Test Failed"
 exit -1
fi

# Compare hashes (Rerecord)
if [ "${baseHashRerecord}" = "${newHashRerecord}" ]; then
 echo "[] Rerecord Test Passed"
 exit 0
else
 echo "[] Rerecord Test Failed"
 exit -1
fi
