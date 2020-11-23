import sys

f = open(sys.argv[1])

line = f.readline()

for i in range(len(line)):
        if (i%32==31):
                    print(line[i-31:i+1]);


