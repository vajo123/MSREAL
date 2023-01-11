#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/semaphore.h>
#define BUFF_SIZE 100
MODULE_LICENSE("Dual BSD/GPL");

dev_t my_dev_id;
static struct class *my_class;
static struct device *my_device;
static struct cdev *my_cdev;

DECLARE_WAIT_QUEUE_HEAD(writeQ);

struct semaphore sem;
char stred[BUFF_SIZE];
int pos = 0;
int endRead = 0;

int stred_open(struct inode *pinode, struct file *pfile);
int stred_close(struct inode *pinode, struct file *pfile);
ssize_t stred_read(struct file *pfile, char __user *buffer, size_t length, loff_t *offset);
ssize_t stred_write(struct file *pfile, const char __user *buffer, size_t length, loff_t *offset);

struct file_operations my_fops =
{
	.owner = THIS_MODULE,
	.open = stred_open,
	.read = stred_read,
	.write = stred_write,
	.release = stred_close,
};


int stred_open(struct inode *pinode, struct file *pfile) 
{
		printk(KERN_INFO "Succesfully opened stred\n");
		return 0;
}

int stred_close(struct inode *pinode, struct file *pfile) 
{
		printk(KERN_INFO "Succesfully closed stred\n");
		return 0;
}

ssize_t stred_read(struct file *pfile, char __user *buffer, size_t length, loff_t *offset) 
{
	int ret;
	char buff[BUFF_SIZE];
	long int len;
	if (endRead){
		endRead = 0;
		printk(KERN_INFO "Succesfully read from stred\n");
		return 0;
	}
	if(pos > 0)
	{
		len = scnprintf(buff, BUFF_SIZE, "%s\n", stred);
		ret = copy_to_user(buffer, buff, len);
		if(ret)
			return -EFAULT;
		endRead = 1;
	}
	else
	{
		printk(KERN_INFO "Stred is empty!\n");
		return 0;
	}

	return len;
}

ssize_t stred_write(struct file *pfile, const char __user *buffer, size_t length, loff_t *offset) 
{
	char buff[150];
	char podatak[BUFF_SIZE];
	char funk[8];
	int ret;
	int duzina;

	ret = copy_from_user(buff, buffer, length);
	if(ret)
		return -EFAULT;
	buff[length-1] = '\0';

	ret = sscanf(buff,"%10[^= ]=%99[^\t\n=]", funk, podatak);

	printk(KERN_INFO "Funkcija je :%s\n", funk);

	if(ret == 2)
	{
		printk(KERN_INFO "Podatak je :%s\n", podatak);
	}
	else if(ret == 1)
	{
		printk(KERN_INFO "Nema podatka\n");
	}


//STRING
	if(!strcmp(funk,"string"))
	{
		pos = 0;
		duzina = strlen(podatak);
		if(duzina < 100)
		{
			strncpy(stred,podatak,duzina);
			printk(KERN_INFO "Podatak je uspesno ubacen u stred\n");
			stred[duzina] = '\0';
			pos = duzina;
		}
		else
		{
			printk(KERN_WARNING "Podatak je preveliki!!!\n");
		}
		printk(KERN_INFO "Funkcija string je gotova!\n");
	}


//CLEAR
	if(!strcmp(funk,"clear"))
	{
		pos = 0;
		stred[0] = '\0'; 
		printk(KERN_INFO "Funkcija clear je gotova!\n");
		
	} 


//SHRINK
	if(!strcmp(funk,"shrink"))
	{
		int i;
		
		if(down_interruptible(&sem))
			return -ERESTARTSYS;		

		if(stred[pos-1] == ' ')  
		{
			stred[pos-1] = '\0';
		}
		
		if(stred[0] == ' ')
		{	
			for(i = 1; i <= pos; i++){
				stred[i-1] = stred[i];
			}
		}
		printk(KERN_INFO "Funkcija shrink je gotova!\n");

		up(&sem);
		wake_up_interruptible(&writeQ);
	}


//APPEND
	if(!strcmp(funk,"append"))
	{
		int i;	
		duzina = strlen(podatak);
		
		if(down_interruptible(&sem))
			return -ERESTARTSYS;
		while((pos + duzina ) >= 100)
		{
			up(&sem);
			if(wait_event_interruptible(writeQ,((pos + duzina) < 100)))
				return -ERESTARTSYS;
			if(down_interruptible(&sem))
				return -ERESTARTSYS;
		}

		if((pos + duzina) < 100)
		{
			for(i = 0; i < duzina; i++)
			{
				stred[pos+i] = podatak[i];
			}
			pos+=duzina;
			stred[pos] = '\0';
		}
		else
		{
			printk(KERN_WARNING "Ne moze se dodati podatak;nema dovoljno slobodnih mesta u baferu!!!\n");
		}

		printk(KERN_INFO "Funkcija append je gotova!\n");
		
		up(&sem);
	}


//TRUNCATE
	if(!strcmp(funk,"truncat"))
	{
		int i;
		char *pom;
		i = simple_strtol(podatak,&pom,10);
		//printk(KERN_INFO "Broj je :%d\n",i);
		//printk(KERN_INFO "Srting  je :%s\n",pom);

   		if(down_interruptible(&sem))
			return -ERESTARTSYS;

		if((pos - i) < 0)
		{
			pos = 0;
			stred[0] = '\0';
			printk(KERN_INFO "Trazili ste da se obrise vise nego sto ima u baferu!\n");	
		}
		else
		{
			pos-=i;
			stred[pos] = '\0';
			printk(KERN_INFO "Funkcija truncate je dotova!\n");
		}
		
		up(&sem);
		wake_up_interruptible(&writeQ);	
	}


//REMOVE
	if(!strcmp(funk,"remove"))
	{
		int i,j,pom,k,n;
		int tacno = 0;
		duzina = strlen(podatak);
		//printk(KERN_INFO "POS JE:%d\n",pos);

		if(down_interruptible(&sem))
			return -ERESTARTSYS;

		for(i = 0; i < pos; i++)
		{
			if(podatak[0] == stred[i])
			{	
				pom = i+1;
				for(j = 1; j < duzina; j++)
				{
					if(podatak[j] == stred[pom])
						tacno++;
					pom++;
					//printk(KERN_INFO "POM JE:%d\n",pom);
				}
				//printk(KERN_INFO "TACNO JE:%d\n",tacno);
				
				if(tacno == duzina-1)
				{	
					n = pom;
					//printk(KERN_INFO "N je :%d\n",n);

					for(k = 0; k < (pos - pom + 1); k++)
					{
						stred[n - duzina] = stred[n];
						n++;
					}
					pos-=duzina;

					printk(KERN_INFO "Funkcija remove je gotova!\n");
				}
				tacno = 0;
			}			
		}
		
		up(&sem);
	}
	

	
	return length;
}

static int __init stred_init(void)
{
   int ret = 0;
	
   sema_init(&sem,1);

   ret = alloc_chrdev_region(&my_dev_id, 0, 1, "stred");
   if (ret){
      printk(KERN_ERR "failed to register char device\n");
      return ret;
   }
   printk(KERN_INFO "char device region allocated\n");

   my_class = class_create(THIS_MODULE, "stred_class");
   if (my_class == NULL){
      printk(KERN_ERR "failed to create class\n");
      goto fail_0;
   }
   printk(KERN_INFO "class created\n");
   
   my_device = device_create(my_class, NULL, my_dev_id, NULL, "stred");
   if (my_device == NULL){
      printk(KERN_ERR "failed to create device\n");
      goto fail_1;
   }
   printk(KERN_INFO "device created\n");

	my_cdev = cdev_alloc();	
	my_cdev->ops = &my_fops;
	my_cdev->owner = THIS_MODULE;
	ret = cdev_add(my_cdev, my_dev_id, 1);
	if (ret)
	{
      printk(KERN_ERR "failed to add cdev\n");
		goto fail_2;
	}
   printk(KERN_INFO "cdev added\n");
   printk(KERN_INFO "Hello world\n");

   return 0;

   fail_2:
      device_destroy(my_class, my_dev_id);
   fail_1:
      class_destroy(my_class);
   fail_0:
      unregister_chrdev_region(my_dev_id, 1);
   return -1;
}

static void __exit stred_exit(void)
{
   cdev_del(my_cdev);
   device_destroy(my_class, my_dev_id);
   class_destroy(my_class);
   unregister_chrdev_region(my_dev_id,1);
   printk(KERN_INFO "Goodbye, cruel world\n");
}


module_init(stred_init);
module_exit(stred_exit);
