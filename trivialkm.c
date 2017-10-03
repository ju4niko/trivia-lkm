/**
 * @file   trivialkm.c
 * @author Juan A. Montenegro	
 * @date   26 Oct 2016
 * @version 0.1
 * @brief   Diver LKM para BeagelBone Black que utiliza dos pulsadores y dos leds
 * conectados a ports GPIO e implementa un juego de preguntas y respuestas
 * el archivo con las preguntas y sus respuestas se ingresa por medio de sysfs
  * @see repo del curso en GIT
 */

#include <linux/init.h>           // Macros para funciones ej. __init __exit
#include <linux/module.h>         // Core header para carga de LKMs en el kernel
#include <linux/device.h>         // Header soporte del kernel Driver Model
#include <linux/kernel.h>         // types, macros, y funciones para el kernel
#include <linux/fs.h>             // Header para soporte del filesys Linux
#include <linux/gpio.h>           // soporte de uso de GPIO
#include <linux/interrupt.h>      // soporte de uso de IRQ
#include <asm/uaccess.h>          // requerido para la funcion de copia al usuario

#define  DEVICE_NAME "trivialkm"  ///< el dispositivo aparece con este nombre en /dev
#define  CLASS_NAME  "fslkm"      ///< nombre de la clase de dispositivo en el sysfs

MODULE_LICENSE("GPL");            ///< Tipo de licencia
MODULE_AUTHOR("Juan A. Montenegro");    ///< Autor, visible con modinfo
MODULE_DESCRIPTION("Juego de trivia con botones y luces");  ///< Descripcion visible con modinfo
MODULE_VERSION("0.1");            ///< Numero de versionado

static int    majorNumber;                  ///< Almacena el numero mayor de device, se determina automaticamente en este ejemplo
static char   message[256] = {0};           ///< Memoria para la string de los mensajes de pregunta y respuesta hacia espaci de usaurio
static int    size_of_message;              ///< Para el mensaje
static int    numberPresses = 0;            ///< acumulador de botonazos
static struct class*  triviaClass  = NULL; 	///< puntero a device-driver class struct 
static struct device* triviaDevice = NULL; 	///< puntero a device-driver device struct
static unsigned int irq1;          			///< irq para el boton 1
static unsigned int irq2;          			///< irq para el boton 2
static unsigned char boton;					///< nro de boton para devolver en read()

static unsigned int gpioR1 = 49;       ///< Harcodeamos los leds a pines de gpio
static unsigned int gpioR2 = 44;       ///< Harcodeamos los leds a pines de gpio
static unsigned int gpioV1 = 48;       ///< Harcodeamos los leds a pines de gpio
static unsigned int gpioV2 = 45;       ///< Harcodeamos los leds a pines de gpio
static unsigned int gpioA1 = 20;       ///< Harcodeamos los leds a pines de gpio
static unsigned int gpioA2 = 47;       ///< Harcodeamos los leds a pines de gpio

static unsigned int gpioBot1 = 60;   ///< tambien los botones pulsadores
static unsigned int gpioBot2 = 61;   ///< 

static bool     Rojo1On = 0;		///< colores de los leds: azul=standby, verde=gano pulseada, rojo=perdio
static bool     Rojo2On = 0;
static bool     Verde1On = 0;
static bool     Verde2On = 0;
static bool     Azul1On = 0;
static bool     Azul2On = 0;



// Prototipado de funciones a implementar en el driver -- debe estar antes de la definicion de estructura file operations
static int     dev_open(struct inode *, struct file *);
static int     dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

// y las de interrupcion
static irq_handler_t triviaLKM_irq1_handler(unsigned int irq, void *dev_id, struct pt_regs *regs);
static irq_handler_t triviaLKM_irq2_handler(unsigned int irq, void *dev_id, struct pt_regs *regs);

/** @brief Estructura de definicion de funciones a implementar
 *  definida en  /linux/fs.h 
 *  usualmente solo implementamos open() read() write() y release()
 */
static struct file_operations fops =
{
   .open = dev_open,	// prepara leds para inicio (standby)
   .read = dev_read,	// lee tiempos de presion de los botones (no implementada aun)
   .write = dev_write,	// manda pregunta de trivia para mostrar por kern.log
   .release = dev_release, // apaga todo 
};

static struct task_struct *sleeping_task = NULL; 	//para despertar el read por interrupcion
							//es un puntero a task_struct, lo uso para guardar el "current" 
							//que es el puntero del kernel a la task_struct del 
							//proceso que esta actualmente en ejecucion, o sea el que 
							//invoco a este lkm

/** @brief Funcion de inicializacion del LKM
 *  El "static" restringe la visibilidad de la funcion dentro de este fuente. La macro __init
 *  entiende que para un driver built-in (no un LKM) la funcion solo se usa para el momento de inicializacion,
 *  puede ser descartada y su memoria liberada luego de este punto
 *  @return retorna 0 si esta OK
 */
static int __init trivia_init(void){
  
   int result = 0; // para recoger el resultado de los pedidos de regreso de las IRQs
  
   printk(KERN_INFO "TriviaLKM: Inicializando TriviaDriver...\n");

   // tratamos de determinar un MAJOR number automaticamente 
   majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
   if (majorNumber<0){
      printk(KERN_ALERT "TriviaLKM falla al intentar registrar nro mayor\n");
      return majorNumber;
   }
   printk(KERN_INFO "TriviaLKM: registrado correctamente con nro mayor %d\n", majorNumber);

   // registramos la calss del dispositivo
   triviaClass = class_create(THIS_MODULE, CLASS_NAME);
   if (IS_ERR(triviaClass)){                // chequeo de error y cleanup si falla
      unregister_chrdev(majorNumber, DEVICE_NAME);
      printk(KERN_ALERT "Failed to register device class\n");
      return PTR_ERR(triviaClass);          // forma correcta de devolver un puntero como error
   }
   printk(KERN_INFO "TriviaLKM: clase de dispositivo registrada correctamente\n");

   // Register the device driver
   triviaDevice = device_create(triviaClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
   if (IS_ERR(triviaDevice)){               // chequeo de error y cleanup si falla
      class_destroy(triviaClass);           // este codigo se repite pero la alternativa es hacer goto....en fin
      unregister_chrdev(majorNumber, DEVICE_NAME);
      printk(KERN_ALERT "falla al crear el dispositivo\n");
      return PTR_ERR(triviaDevice);
   }
   printk(KERN_INFO "TriviaLKM: dispositivo creado correcatmente\n"); // inicializado OK!
   
   // ahora vamos a reservar recursos de hardware en la funcion de init porque seran de uso exclusivo
   //
   // reservar 2 irq para los botones
   // se deberia chequear con la funcion gpio_is_valid(nro de gpio) a ver si se puede usar cada pin
   // hay que configurar 6 pines de leds, primera version solo los led rojos
   
   // preparamos los leds
   Rojo1On = false;
   Rojo2On = false;
   Verde1On = false;
   Verde2On = false;
   Azul1On = false;
   Azul2On = false;
   
   gpio_request(gpioR1, "sysfs");          // le pedimos al sysfs el mapeo del led
   gpio_request(gpioR2, "sysfs");          // le pedimos al sysfs el mapeo del led
   gpio_request(gpioV1, "sysfs");          // le pedimos al sysfs el mapeo del led
   gpio_request(gpioV2, "sysfs");          // le pedimos al sysfs el mapeo del led
   gpio_request(gpioA1, "sysfs");          // le pedimos al sysfs el mapeo del led
   gpio_request(gpioA2, "sysfs");          // le pedimos al sysfs el mapeo del led
   
   
   gpio_direction_output(gpioR1, false);   // lo configuramos como salida y estado inicial (off)
   gpio_direction_output(gpioR2, false);   // lo configuramos como salida y estado inicial (off)
   gpio_direction_output(gpioV1, false);   // lo configuramos como salida y estado inicial (off)
   gpio_direction_output(gpioV2, false);   // lo configuramos como salida y estado inicial (off)
   gpio_direction_output(gpioA1, false);   // lo configuramos como salida y estado inicial (off)
   gpio_direction_output(gpioA2, false);   // lo configuramos como salida y estado inicial (off)
   
   
// gpio_set_value(gpioR1, false);          // no es necesario ahora, lo pongo por referencia

   gpio_export(gpioR1, false);             // hace gpioXX aparecer en /sys/class/gpio
   gpio_export(gpioR2, false);             // hace gpioXX aparecer en /sys/class/gpio
   gpio_export(gpioV1, false);             // hace gpioXX aparecer en /sys/class/gpio
   gpio_export(gpioV2, false);             // hace gpioXX aparecer en /sys/class/gpio
   gpio_export(gpioA1, false);             // hace gpioXX aparecer en /sys/class/gpio
   gpio_export(gpioA2, false);             // hace gpioXX aparecer en /sys/class/gpio
   
                     // el argumento bool = false hace que no se pueda cambiar la direccion
                     
   gpio_request(gpioBot1, "sysfs");       // lo mismo con los botones ahora
   gpio_request(gpioBot2, "sysfs");       // lo mismo con los botones ahora
   
   gpio_direction_input(gpioBot1);        // los hacemos entrada
   gpio_direction_input(gpioBot2);        // los hacemos entrada
   
   
   //gpio_set_debounce(gpioButton, 200);      // podriamos debouncearlo pero para esta aplicacion mejor no
   
   gpio_export(gpioBot1, false);          // lo hacemos aparecer en  /sys/class/gpio
   gpio_export(gpioBot2, false);          // lo hacemos aparecer en  /sys/class/gpio
   
   // Podriamos verificar el estado de los botones
   //printk(KERN_INFO "triviaLKM: Estado de boton1: %d\n", gpio_get_value(gpioBot1));
   //printk(KERN_INFO "triviaLKM: Estado de boton2: %d\n", gpio_get_value(gpioBot2));
   
   // como los nros de GPIO e IRQ no son coincidentes, los pedimos con una funcion de mapeo
   irq1 = gpio_to_irq(gpioBot1);
   irq2 = gpio_to_irq(gpioBot2);
   
   
   printk(KERN_INFO "triviaLKM: Boton1 en IRQ: %d\n", irq1);
   printk(KERN_INFO "triviaLKM: Boton2 en IRQ: %d\n", irq2);
 

  //antes que se habilite el uso de irqs en el handler
  //preparo el puntero a la tarea que esta usando el driver para hacerla dormir
  sleeping_task = NULL;	

   
   // pedimos las IRQ correspondientes - CHEQUEAR ESTO TAMBIEN!!!
   result = request_irq(irq1,             // la irq pedida
                        (irq_handler_t) triviaLKM_irq1_handler, // puntero a la funcion handler
                        IRQF_TRIGGER_RISING,   // flanco de subida
                        "trivia_gpio_handler",    // en /proc/interrupts para identificar al propietario
                        NULL);                 // el *dev_id para irqs compartidas, NULL esta OK aqui
 
   printk(KERN_INFO "triviaLKM: El pedido de IRQ1 es: %d\n", result);

   result = request_irq(irq2,             // la irq pedida
                        (irq_handler_t) triviaLKM_irq2_handler, // puntero a la funcion handler
                        IRQF_TRIGGER_RISING,   // flanco de subida
                        "trivia_gpio_handler",    // en /proc/interrupts para identificar al propietario
                        NULL);                 // el *dev_id para irqs compartidas, NULL esta OK aqui
 
   printk(KERN_INFO "triviaLKM: El pedido de IRQ2 es: %d\n", result);
   
   return 0;
}

// handlers de las IRQs
static irq_handler_t triviaLKM_irq1_handler(unsigned int irq, void *dev_id, struct pt_regs *regs){
  
  //al llegar esta int pongo en Verde1 y le anulo el computo al otro boton
  
  if (!Azul1On){ //si no esta en stand by
   
    // siempre y cuando el otro no halla llegado antes que yo! (bot1)
    if (!Verde2On) {
	Rojo1On = false;
	Azul1On = false;
	Verde1On = true;
	//y le enciendo el rojo!
	Rojo2On=true;
      
	gpio_set_value(gpioR1, Rojo1On);
	gpio_set_value(gpioR2, Rojo2On); 
	gpio_set_value(gpioV1, Verde1On);
	gpio_set_value(gpioV2, Verde2On); 
	gpio_set_value(gpioA1, Azul1On);
	gpio_set_value(gpioA2, Azul2On); 
	
	// y por ultimo, despierto al usuario haciendo read()
	// siempre y cuando no sea NULL pointer
	if ( sleeping_task == NULL) {
	    //nada
	}else{
	    boton = 1;
	    wake_up_process ( sleeping_task );
	}
      }
    }
    printk(KERN_INFO "triviaLKM: Interrupcion1! (el estado del boton es %d)\n", gpio_get_value(gpioBot1));
    numberPresses++;                         // acumulador de cantidad de interrups, es informativo
    return (irq_handler_t) IRQ_HANDLED;      // le avisa al kernel que la IRQ se vectorizo OK
}

static irq_handler_t triviaLKM_irq2_handler(unsigned int irq, void *dev_id, struct pt_regs *regs){
  
  if (!Azul2On){ //si no esta en stand by
    //inverso al del otro boton
    if (!Verde1On) {
	Verde2On = true;
	Rojo2On = false;
	Azul2On = false;
	//y le enciendo el rojo!
	Rojo1On=true;
	
	gpio_set_value(gpioR1, Rojo1On);
	gpio_set_value(gpioR2, Rojo2On); 
	gpio_set_value(gpioV1, Verde1On);
	gpio_set_value(gpioV2, Verde2On); 
	gpio_set_value(gpioA1, Azul1On);
	gpio_set_value(gpioA2, Azul2On);
	
	// y por ultimo, despierto al usuario haciendo read()
	// siempre y cuando no sea NULL pointer
	if (sleeping_task==NULL) {
	    //nada
	}else{
	    boton = 2;
	    wake_up_process(sleeping_task);
	}
    }
  }
  printk(KERN_INFO "triviaLKM: Interrupcion2! (el estado del boton es %d)\n", gpio_get_value(gpioBot1));
  numberPresses++;                         // acumulador de cantidad de interrups, es informativo
  return (irq_handler_t) IRQ_HANDLED;      // le avisa al kernel que la IRQ se vectorizo OK	
}

/** @brief Funcion de remocion del LKM
 *  Similar a la de inicializacion, tambien static. La macreo __exit notifica que si este 
 *  codigo es utilizado por unj driver built-in (no un LKM) esta funcion no es requerida.
 */
static void __exit trivia_exit(void){
		
		
   printk(KERN_INFO "triviaLKM:se apretaron los botones %d veces\n", numberPresses);
   // apago todo
   gpio_set_value(gpioR1, 0);
   gpio_set_value(gpioR2, 0);
   gpio_set_value(gpioV1, 0);
   gpio_set_value(gpioV2, 0);
   gpio_set_value(gpioA1, 0);
   gpio_set_value(gpioA2, 0);
   
   gpio_unexport(gpioR1);                  // desconecto del sysfs
   gpio_unexport(gpioR2);                  // desconecto del sysfs
   gpio_unexport(gpioV1);                  // desconecto del sysfs
   gpio_unexport(gpioV2);                  // desconecto del sysfs
   gpio_unexport(gpioA1);                  // desconecto del sysfs
   gpio_unexport(gpioA2);                  // desconecto del sysfs
   gpio_unexport(gpioBot1);               // Unexporto los botones
   gpio_unexport(gpioBot2);               // Unexporto los botones
   
   free_irq(irq1, NULL);               // libero las irq solicitadas
   free_irq(irq2, NULL);               // libero las irq solicitadas
   
   gpio_free(gpioR1);                      // libero leds
   gpio_free(gpioR2);                      // libero leds
   gpio_free(gpioV1);                      // libero leds
   gpio_free(gpioV2);                      // libero leds
   gpio_free(gpioA1);                      // libero leds
   gpio_free(gpioA2);                      // libero leds
   
   gpio_free(gpioBot1);                   // libero botones
   gpio_free(gpioBot2);                   // 
   
   device_destroy(triviaClass, MKDEV(majorNumber, 0));     // remuevo el objeto de la clase
   class_unregister(triviaClass);                          // desregisto la clase del dispositivo
   class_destroy(triviaClass);                             // remuevo la clase del dispositivo
   unregister_chrdev(majorNumber, DEVICE_NAME);             // desregistro el nro mayor
   printk(KERN_INFO "TriviaLKM: dispositivo desinstalado OK!\n");
}

//Funcion open
static int dev_open(struct inode *inodep, struct file *filep){
  //encendemos leds azules, esto prepara para iniciar secuencia de juego
  //apagamos el resto
  
  Rojo1On = false;
  Azul1On = true;
  Verde1On = false;
  Rojo2On = false;
  Azul2On = true;
  Verde2On = false;
 
  gpio_set_value(gpioR1, Rojo1On);
  gpio_set_value(gpioR2, Rojo2On); 
  gpio_set_value(gpioV1, Verde1On);
  gpio_set_value(gpioV2, Verde2On); 
  gpio_set_value(gpioA1, Azul1On);
  gpio_set_value(gpioA2, Azul2On); 

  //preparo el puntero a la tarea que esta usando el driver para hacerla dormir
  sleeping_task = NULL;	
   
   return 0;
}

// Llamada por read(), apaga leds azules y habilita el juego

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset){
   int error_count = 0;
   
  //apagamos todo
  Rojo1On = false;
  Azul1On = false;
  Verde1On = false;
  Rojo2On = false;
  Azul2On = false;
  Verde2On = false;
  gpio_set_value(gpioR1, Rojo1On);
  gpio_set_value(gpioR2, Rojo2On); 
  gpio_set_value(gpioV1, Verde1On);
  gpio_set_value(gpioV2, Verde2On); 
  gpio_set_value(gpioA1, Azul1On);
  gpio_set_value(gpioA2, Azul2On); 
  
  
  sleeping_task = current;		//le indico el proceso que esta usando el driver 
  set_current_state(TASK_INTERRUPTIBLE);//lo pongo a dormir y que se pueda interrumpir
  schedule();				//y llamo al scheduler del kernel para que despache otra tarea
  //sigue por aca al llegar una interrupcion por cualquiera de los dos botones
  // ya que las rutinas de atencion llaman a la macro wake_up_process()
  
  sleeping_task = NULL;	// por si llega otra interrupcion  
  
 
  //cargo en el message el identificador del boton apretado
  sprintf(message,"Boton%d\0",boton);
  size_of_message = sizeof(message);
   // copy_to_user tiene el formato ( * to, *from, size) y devuelve 0 si es OK
   error_count = copy_to_user(buffer, message, size_of_message);

   if (error_count==0){            // si estuvo todo OK
      printk(KERN_INFO "TriviaLKM: Enviados %d bytes al usuario\n", size_of_message);
      return (size_of_message);
   }
   else {
      printk(KERN_INFO "TriviaLKM: Falla al enviar %d bytes al usuario\n", error_count);
      return -EFAULT;              // falla, devuelve BAD ADDRESS (i.e. -14)
   }
}

// Llamada por write()
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset){
	
	// aqui se deberia usar copy_from_user
	
   sprintf(message, "%s(%d letters)", buffer, len);   // appending received string with its length
   size_of_message = strlen(message);                 // store the length of the stored message
   printk(KERN_INFO "TriviaLKM: Recibidos %d chars del usuario\n", len);
   return len;
}


// la funcion que llama el usuario para "cerrar" el dispositivo


static int dev_release(struct inode *inodep, struct file *filep){
  
  //apagamos todo
  Rojo1On = false;
  Azul1On = false;
  Verde1On = false;
  Rojo2On = false;
  Azul2On = false;
  Verde2On = false;
  
   gpio_set_value(gpioR1, Rojo1On);
   gpio_set_value(gpioR2, Rojo2On); 
   gpio_set_value(gpioV1, Verde1On);
   gpio_set_value(gpioV2, Verde2On); 
   gpio_set_value(gpioA1, Azul1On);
   gpio_set_value(gpioA2, Azul2On); 


  printk(KERN_INFO "TriviaLKM: Dispositivo cerrado correcatmente\n");
  return 0;
}


  // Un modulo debe usar las macros module_init() module_exit() de linux/init.h, 
  
  //  que identifican las funciones de instalacion y remocion 

module_init(trivia_init);
module_exit(trivia_exit);

