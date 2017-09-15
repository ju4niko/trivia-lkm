/**
 * @file   trivia.c
 * @author Juan A. Montenegro	
 * @date   26 Oct 2016
 * @version 0.1
 * @brief   Programa de usuario del Diver LKM para BeagelBone Black que utiliza dos pulsadores y dos leds
 * conectados a ports GPIO e implementa un juego de preguntas y respuestas
 * el archivo con las preguntas y sus respuestas se carga del filesystem
 * mediante un shell script para imprimir la pregunta y luego otro para la respuesta
 * se llaman pregunta.sh y respuesta.sh respectivamente
 * @see repo del curso en SVN
 */

#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<fcntl.h>
#include<string.h>
#include<time.h>

int main (void){
  
  int fdlkm, fdtrv; //para el device y para el archivo con las preguntas
  int r;
  unsigned char s;
  char buffer[256];
  char comando[256];
  
  fdlkm = open ("/dev/trivialkm", O_RDWR);             // abrimos para lectura/escritura
  if (fdlkm < 0){
    perror("Falla al abrir dev file...");
    return errno;
  }
  printf("\nPresione ENTER para iniciar el juego\n");
  getchar();
  s=(unsigned int)time(NULL);
  s = s/10;
  sprintf(comando,"/home/tdiii/lkms/trivia/pregunta.sh %02d:",s); 
  printf("%s\n",comando);
  //leo el archivo con las preguntas
  // mediante un shell script
  system(comando);

  //con el read lo pongo listo para empezar
  //debe ser bloqueante hasta que se presiones un boton
  r = read (fdlkm, buffer, 7); 	//y debe destrancar a la primera interrupcion
				//En el buffer devuelve el jugador que apreto primero el boton
  if (r>0){
    printf("\nPrimero se presiono: %s\n",buffer);
  } else {
    perror("Error de lectura");
    return errno;
  }
  printf("\nPresione ENTER para la respuesta correcta\n");
  getchar();
  sprintf(comando,"/home/tdiii/lkms/trivia/respuesta.sh %02d:",s); 
  printf("%s\n",comando);
  //leo el archivo con las respuestas
  // mediante un shell script
  system(comando);

    
  close(fdlkm);
  
  
  return 0;
}
