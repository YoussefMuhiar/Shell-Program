all:
	gcc msh.c -o msh -std=c99
clean:
	rm msh msh-shell.tgz
submission:
	tar czvf msh.tgz Makefile msh.c msh 
