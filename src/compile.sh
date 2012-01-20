#!/bin/bash

TARGET=../experiments/benchmark
LIB=libhoard.so

make 

cp $LIB    $TARGET
