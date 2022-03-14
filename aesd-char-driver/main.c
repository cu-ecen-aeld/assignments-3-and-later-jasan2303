/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>  //provides definations for cdev for registring the char device
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Jasan Singh"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
	PDEBUG("open");
	/**
	 * TODO: handle open
	 */
	 
       struct aesd_dev *dev; /* device information */                  //also think to free it up during cleanup!
       dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
       filp->private_data = dev; /* for other methods */ 
       
	return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
	PDEBUG("release");
	/**
	 * TODO: handle release
	 */
	return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
	ssize_t retval = 0;
	PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
	/**
	 * TODO: handle read
	 */
	
	struct aesd_dev *dev = filp->private_data;
	
	//struct scull_qset *dptr; /* the first listitem
	struct *aesd_buffer_entry;      //current entry
	size_t *entry_offset_byte_rtn;  //offset w.r.t current entry
	int quantum;
	
	if (down_interruptible(&dev->sem))
          return -ERESTARTSYS;
	
	*aesd_buffer_entry = aesd_circular_buffer_find_entry_offset_for_fpos(dev->aesd_buffer,
			         *f_pos, entry_offset_byte_rtn );
        
        
        if(!*entry_offset_byte_rtn)
	   quantum = (dev->aesd_buffer->entry[out_offs]->size - *entry_offset_byte_rtn +1) ;  //current read element length
	else
	   quantum = dev->aesd_buffer->entry[out_offs]->size;
	
	if(count > quantum)
	  count = quantum;
	  
	if (*f_pos >= dev->size)
         goto out;
       
      // if (*f_pos + count > dev->size)  //if the required count is greater than the total available date the requested count is updated 
       //   count = dev->size - *f_pos;    //the current available length
	
	if( copy_to_user(buf, f_pos, count)) 
	{
          retval = -EFAULT;
          goto out;
        }
	
	*f_pos = *f_pos +count;
	retval = count;
 
   out:
       up(&dev->sem);
       return retval; 
	 
	
	return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
	ssize_t retval = -ENOMEM;
	PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
	/**
	 * TODO: handle write
	 */
	 
	struct aesd_dev *dev = filp->private_data;
	size_t tcount=0; //for temp store count till \n
	void * command_end;
	
  again:
	command_end=memchr(buf, '\n', count);
	
	if( command_end == NULL)   //no newline find 
	{
	  dev->buf_entry->buffptr= kmalloc(count, GFP_KERNEL);
	  copy_from_user(buf, dev->buf_entry->buffptr, count);
	  dev->buf_entry->size += count;
	}
	else
	{
	  tcount = command_end - buf; //count till the '\n'
	  dev->buf_entry->buffptr= kmalloc(count, GFP_KERNEL);
	  copy_from_user(buf, dev->buf_entry->buffptr, count);
	  dev->buf_entry->size += count;
	  
	  //buf
	  //adding the completed entry to the buffer
	  aesd_circular_buffer_add_entry( dev->aesd_buffer, dev->buf_entry);
	  
	  //free entry
	  kfree(dev->buf_entry);
	  count -= tcount;
	  
	  if( count != 0)
	   goto again;
	}
	 
	 retval = count;
	
	return retval;
}
struct file_operations aesd_fops = {                     //file operations used by the device
	.owner =    THIS_MODULE,
	.read =     aesd_read,
	.write =    aesd_write,
	.open =     aesd_open,
	.release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
	int err, devno = MKDEV(aesd_major, aesd_minor);  //getting the device number for oour device

	cdev_init(&dev->cdev, &aesd_fops);        //initializing the baasic kernel structure cdev 
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &aesd_fops;
	err = cdev_add (&dev->cdev, devno, 1);    // adding/telling the kernel about the device, currently associated only one device number
	if (err) {
		printk(KERN_ERR "Error %d adding aesd cdev", err);
	}
	return err;
}



int aesd_init_module(void)
{
	dev_t dev = 0;
	int result;
	result = alloc_chrdev_region(&dev, aesd_minor, 1,
			"aesdchar");
	aesd_major = MAJOR(dev);
	if (result < 0) {
		printk(KERN_WARNING "Can't get major %d\n", aesd_major);
		return result;
	}
	memset(&aesd_device,0,sizeof(struct aesd_dev));

	/**
	 * TODO: initialize the AESD specific portion of the device
	 */
         
         aesd_circular_buffer_init(aesd_device->aesd_buffer);   //initializing the circular buffer
         //aaesd_device->buf_entry??              //also need to intialize single entry for writing


	result = aesd_setup_cdev(&aesd_device);

	if( result ) {
		unregister_chrdev_region(dev, 1);
	}
	return result;

}

void aesd_cleanup_module(void)
{
	dev_t devno = MKDEV(aesd_major, aesd_minor);

	cdev_del(&aesd_device.cdev);

	/**
	 * TODO: cleanup AESD specific poritions here as necessary
	 */

	unregister_chrdev_region(devno, 1);
}
    


module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
