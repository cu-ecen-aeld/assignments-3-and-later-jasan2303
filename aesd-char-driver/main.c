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
#include <linux/slab.h> //kfree
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Jasan Singh"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{

       struct aesd_dev *dev; /* device information */                  //also think to free it up during cleanup!
	PDEBUG("open");
	/**
	 * TODO: handle open
	 */
	 
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
	struct aesd_dev *dev = (struct aesd_dev *)filp->private_data;
	
	//struct scull_qset *dptr; /* the first listitem
	struct aesd_buffer_entry *curr_entry =NULL;      //current entry
	ssize_t entry_offset_byte_rtn;  //offset w.r.t current entry
	ssize_t quantum;
	
	PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
	/**
	 * TODO: handle read
	 */
	
	if(mutex_lock_interruptible(&dev->dev_mutex))
           return -ERESTARTSYS;
	
	curr_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->aesd_buffer,
			         *f_pos, &entry_offset_byte_rtn );
        if( curr_entry == NULL)
         goto out;
        
   
	quantum = curr_entry->size - entry_offset_byte_rtn;
	if( quantum > count)
	  quantum = count;
	  
	
	if( copy_to_user(buf, curr_entry->buffptr + entry_offset_byte_rtn, quantum)) 
	{
          retval = -EFAULT;
          goto out;
        }
	
	*f_pos = *f_pos + quantum;
	retval = quantum;
 
   out:
       mutex_unlock(&dev->dev_mutex);
       return retval; 
	 
	
	return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
	ssize_t retval = -ENOMEM;
	struct aesd_dev *dev = (struct aesd_dev *) filp->private_data;
	//size_t tcount=0; //for temp store count till \n
	ssize_t writen=0;
	const char * ret_entry = NULL;
	PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
	/**
	 * TODO: handle write
	 */
	
	//command_end=memchr(buf, '\n', count);
	
         if(mutex_lock_interruptible(&dev->dev_mutex))
           return -ERESTARTSYS;
         
         if(dev == NULL)
	    goto out;
	    
	 if( !dev->buf_entry.size)
	  { 
	     dev->buf_entry.buffptr= kmalloc(count * sizeof(char *), GFP_KERNEL);
	  }
	  else
	  {
	     dev->buf_entry.buffptr= krealloc( dev->buf_entry.buffptr, (dev->buf_entry.size + count), GFP_KERNEL);
	  }
	  
	  writen= copy_from_user( (void *)(&dev->buf_entry.buffptr[dev->buf_entry.size]),buf, count);
	  
	  PDEBUG("%ld " , writen);
	 
          
	 retval = count -writen;
	 dev->buf_entry.size += retval;
	
	
	  if( memchr(dev->buf_entry.buffptr , '\n',dev->buf_entry.size ) != NULL)
	  {
	     ret_entry=aesd_circular_buffer_add_entry( &dev->aesd_buffer, &dev->buf_entry);
	     if( ret_entry != NULL)
	       kfree(ret_entry);
	       
	     dev->buf_entry.size =0; //restting
	     dev->buf_entry.buffptr= NULL;
	  }
	  PDEBUG(" %s ", dev->buf_entry.buffptr);
	
    
    out:
      mutex_unlock(&dev->dev_mutex);
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
         
         aesd_circular_buffer_init(&aesd_device.aesd_buffer);   //initializing the circular buffer
         mutex_init(&(aesd_device.dev_mutex));
        

	result = aesd_setup_cdev(&aesd_device);

	if( result ) {
		unregister_chrdev_region(dev, 1);
	}
	return result;

}

void aesd_cleanup_module(void)
{
	// to use Macro in aeesd-circularbuffer.h
        uint8_t index;
	struct aesd_buffer_entry *dev_entry;
	
	dev_t devno = MKDEV(aesd_major, aesd_minor);

	cdev_del(&aesd_device.cdev);

        
	/**
	 * TODO: cleanup AESD specific poritions here as necessary
	 */
	kfree(aesd_device.buf_entry.buffptr);
	
	AESD_CIRCULAR_BUFFER_FOREACH(dev_entry, &aesd_device.aesd_buffer, index){
		if(dev_entry->buffptr != NULL)
			kfree(dev_entry->buffptr);
	
	}

	unregister_chrdev_region(devno, 1);
}
    


module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
