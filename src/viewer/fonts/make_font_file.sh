#!/usr/bin/env bash

c++ ./binary_to_compressed_c.cpp -o to-binary

echo "#pragma once" > ./dejavu.h
./to-binary -nostatic -base85 "DejaVu Sans Mono for Powerline.ttf" DejaVu >> ./dejavu.h
