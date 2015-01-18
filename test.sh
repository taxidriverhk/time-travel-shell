# This comment should be ignored
:   semicolon test
     : spaces should not matter
: : :

# Simple command tests
echo standard echo
(echo echo in shell || echo this text should not appear)

# Output Redirection
echo this is my first test file > testfiles
cat testfiles
echo overwrite file > testfiles

# Input Redirection
cat < testfiles
cat < FILEDOESNOTEXIST  # should produce an error in both cases

# Simultaneous Output and Input Redirection
sort < testfiles > testSorted

# Nonalpha character tests
expr 1 + 3 - 4 / 2 

# Pipe tests
echo hello world | sort
ls -a | wc  -l
who | sort > testfiles

# cleanup
rm testfiles
rm testSorted

# Same as the first command
true ajflkjdlkslkdjlkff

# Subshell command
(true && echo test subshell command | cat)
