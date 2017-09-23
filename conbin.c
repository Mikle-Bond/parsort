#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

int main()
{
	int num = 0, t, i;
	scanf("%d", &num);
	write(STDOUT_FILENO, &num, sizeof(int));

	for(i = 0; i < num; i += 1) {
		scanf("%d", &t);
		write(STDOUT_FILENO, &t, sizeof(int));
	}
	return 0;
}
		

	
