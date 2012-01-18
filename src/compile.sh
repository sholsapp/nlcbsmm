#!/bin/bash

TARGET=../experiments/benchmark
LIB=libhoard.so
CLIENT=nlcbsmm-dist-client

make 

cp $LIB    $TARGET
cp $CLIENT $TARGET
