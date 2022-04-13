#!usr/bin/python3

import shutil
import os
from os import listdir
from os.path import isfile, join
from termios import N_MOUSE

all_files = [f.split("_") for f in listdir("./") if ".wav" in f]

archives = list({ele[0] for ele in all_files})
move = []

for archive in archives:
    items = [f for f in all_files if archive in f]
    numbers = list({ele [1] for ele in items}) # numbers = sprecher
    tmp = []
    for number in numbers[:-1]:
        tmp.extend([(ele[0]+"_"+ele[1]+"_"+ele[2]) for ele in items if number in ele][::8])
    print(len(tmp))
    move.append(tmp)

for g in move:
    for f in g:
        shutil.move(("./"+f), "../limited")


print(len(move))

        
