#!/bin/bash

TARGET=../experiments/hoard
LIB=libhoard.so
CLIENT=nlcbsmm-dist-client

make 

cp $LIB    $TARGET
cp $CLIENT $TARGET
