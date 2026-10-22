#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LEN 1024

typedef struct Node
{
    char *str;
    struct Node *next;
} Node;

int appendNode(Node **head, Node **tail, const char *str)
{
    Node *node = malloc(sizeof(Node));
    if (node == NULL)
        return 0;

    size_t len = strlen(str) + 1;
    node->str = malloc(len);

    if (node->str == NULL)
    {
        free(node);
        return 0;
    }

    memcpy(node->str, str, len);
    node->next = NULL;

    if (*tail == NULL)
        *head = node;
    else
        (*tail)->next = node;

    *tail = node;
    return 1;
}

void printList(Node *head)
{
    while (head != NULL)
    {
        puts(head->str);
        head = head->next;
    }
}

void freeList(Node *head)
{
    while (head != NULL)
    {
        Node *next = head->next;
        free(head->str);
        free(head);
        head = next;
    }
}

int main(void)
{
    char buffer[MAX_LEN];
    Node *head = NULL;
    Node *tail = NULL;
    int status = 0;

    while (1)
    {
        printf("enter string: ");
        fflush(stdout);

        if (fgets(buffer, sizeof(buffer), stdin) == NULL)
        {
            if (ferror(stdin))
            {
                fprintf(stderr, "input error\n");
                status = 1;
            }
            break;
        }

        if (buffer[0] == '.')
            break;

        size_t len = strlen(buffer);

        if (len > 0 && buffer[len - 1] == '\n')
        {
            buffer[len - 1] = '\0';
        }
        else
        {
            int c = getchar();

            if (c != '\n' && c != EOF)
            {
                while (c != '\n' && c != EOF)
                    c = getchar();

                if (ferror(stdin))
                {
                    fprintf(stderr, "input error\n");
                    status = 1;
                    break;
                }

                fprintf(stderr, "string too long\n");
                continue;
            }

            if (ferror(stdin))
            {
                fprintf(stderr, "input error\n");
                status = 1;
                break;
            }
 	       }

	if (strchr(buffer, '\033') != NULL)
	{
            fprintf(stderr, "bad input\n");
	     continue;
	}

        if (!appendNode(&head, &tail, buffer))
        {
            fprintf(stderr, "memory allocation error\n");
            status = 1;
            break;
        }
    }

    if (status == 0)
    {
        printf("\nlist:\n");
        printList(head);
    }

    freeList(head);
    return status;
}
