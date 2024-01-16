# Steps to fix tacopie dependency in cpp_redis library
Need to perform the following on the tacopie submodule in the cpp_redis library to successfully build the project
1. `cd tacopie`
2. `git fetch origin pull/5/head:cmake-fixes`
3. `git checkout cmake-fixes`
4. `cd ..`
