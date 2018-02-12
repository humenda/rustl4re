
   Example for printf, getchar, readln-usage. 
   First, you have to init the lib. 
*/

#include <stdio.h>
#include <string.h>

#include <l4/log/l4log.h>
#include <l4/util/util.h>
#include <l4/sys/kdebug.h>
#include <l4/l4con/l4contxt.h>

char LOG_tag[9] = "txtdemo";
l4_ssize_t l4libc_heapsize = 64 * 1024;

void prompt_test(void);
void scroll_test(void);
void redraw_test(void);

void prompt_test()
{
  char in[100];
  contxt_ihb_t ihb;
  
  LOGl("prompt_test\n");
  while(1)
    {
      printf("mn3@os:");
      contxt_ihb_read((char *) &in, 100, &ihb);
    }
}

void scroll_test()
{
  LOGl("scroll_test\n");
}

void redraw_test()
{
  LOGl("redraw_test\n");
}

int main(void)
{
  char choice;
  char input[200];
  contxt_ihb_t hb;
  int hbs = 11; /* history buffer size in lines */
  
  contxt_init(65536, hbs);
  contxt_ihb_init(&hb, 50, sizeof(input));

  LOGl("contxtdemo started\n");
  while(1)
    {
      printf("enter exit:");
      contxt_ihb_read((char*)&input, sizeof(input),&hb);
      printf("\n");
      if(!strcmp(input,"exit"))
	break;
    }
  return 0;
  /* skip this part */
  while(1)
    {
      LOGl("contxt_clrscr\n");
      contxt_clrscr();
      printf("Hi, this is contxtdemo ...\n\n");
      
      printf(" 1. Prompt + history buffer\n");
      printf(" 2. Scrolling\n");
      printf(" 3. Full screen redraw\n");
      printf(" 4. exit\n");
      printf("Choose your test (1,2 or 3): ");
      LOGl("contxt_ihb_read\n");
      choice = getchar();
      LOGl("choice: %d\n", choice);
      switch(choice) 
	 {
	 case '1':
	   prompt_test();
	   continue;
	 case '2':
	   scroll_test();
	   continue;
	 case '3':
	   redraw_test();
	   continue;
	 case '4':
	   break;
	 default:
	   continue;
	 }
       break;
     }
   
   return 0;
}
