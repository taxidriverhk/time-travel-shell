#!/bin/bash

#	Test time travel dependency
echo command A

echo command B > temp

cat < temp

echo command C

echo command D

rm temp