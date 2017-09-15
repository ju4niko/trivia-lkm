#!/bin/bash

cat preguntas.txt | grep $1 | cut -f 2 -d ":"



