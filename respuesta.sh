#!/bin/bash

cat preguntas.txt | grep $1 | cut -f 3 -d ":"



