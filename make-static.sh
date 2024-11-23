#!/bin/bash

set -e

echo "#pragma once" > src/statics.hpp

echo "constexpr char MAIN_HTML[] = R\"html(" >> src/statics.hpp
cat "main.html" >> src/statics.hpp
echo ")html\";" >> src/statics.hpp
