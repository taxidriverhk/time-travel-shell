#!/bin/bash

#	Sanity Check
#	Multiple Comments

#	Simple command with argument
ls -l

#	Simple command with input
echo This is a simple command > output.txt

#	Simple command with output
echo This is another simple command > output2.txt

echo ouput.txt has been generated

cat < output.txt

#	Command with file dependency
diff output.txt output2.txt

#	Testing AND and OR commands
echo 1 && echo 2

echo 3 || echo 4

#	Clean up
rm output.txt output2.txt

#	Sequence command
echo 5; echo 6; echo sequence || echo OR_COMMAND

#	Pipe command
cat read-command.c | grep void

clear

echo screen cleared

#	Subshell command with I/O redirections
(cat test-p-ok.sh) > test.txt

echo test.txt has been generated