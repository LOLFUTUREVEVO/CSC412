all: CSC412_Final_Proj

CSC412_Final_Proj: Blast_Protocol_Main.c
	gcc Blast_Protocol_Main.c -o bpcp

clean:
	rm -f bpcp
