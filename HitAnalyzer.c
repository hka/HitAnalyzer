#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>

#include "parg.h"
#include "SDL.h"

void list_recording_device()
{
  int i =0;
  int  input_device_count = SDL_GetNumAudioDevices(1);
  for (i = 0; i <  input_device_count; ++i)
  {
    printf("Audio device %d: %s\n", i, SDL_GetAudioDeviceName(i, 1)); 
  }
}

void print_selected_device_name(int32_t id)
{
  int32_t input_device_count = SDL_GetNumAudioDevices(1);
  if(id < 0 || id > input_device_count)
  {
    printf("No device available at: %d\n", id);
  }
  else
  {
    printf("Selected audio device: [%d] %s\n", id, SDL_GetAudioDeviceName(id, 1));
  }
}

struct settings
{
  int32_t selected_device_id;
};

void parse_options(int argc, char* argv[],
		   struct settings* cmd_arg)
{
  // Parse command line args
  struct parg_state ps;
  int32_t option; 
  parg_init(&ps);

    while((option = parg_getopt(&ps, argc, argv, "hs:vld:")) != -1)
  {
    switch(option)
    {
      case 1:
      {
	printf("nonoption '%s'\n", ps.optarg);
	break;
      }
      case 'h':
      {
	printf("Usage: %s [-h] [-v] [-l] [-s STRING] [-d NUMBER]\n\n", argv[0]);
	printf("-l \t list available recording devices\n");
	break;
      }
      case 's':
      {
	printf("option -s with argument '%s'\n", ps.optarg);
	break;
      }
      case 'l':
      {
	list_recording_device();
	break;
      }
      case 'd':
      {
	// Select recording device
	int32_t i;
	bool is_number = true;
	for(i = 0; i < strlen(ps.optarg) && is_number; ++i)
	{
	  is_number = isdigit(ps.optarg[i]) == 0 ? false : true;
	}
	int32_t id = 0;
	if(!is_number)
	{
	  printf("Invalid device id specified, using device 0 instead.\n");
	}
	else
	{
	  id = atoi(ps.optarg);
	}
	cmd_arg->selected_device_id = id;
	break;
      }
      case 'v':
      {
	printf("%s 0.0.1\n", argv[0]);
	break;
      }
      case '?':
      {
	if (ps.optopt == 's') {
	  printf("option -s requires an argument\n");
	}
	else if (ps.optopt == 'd') {
	  printf("option -d requires an argument\n");
	}
	else {
	  printf("unknown option -%c\n", ps.optopt);
	}
	break;
      }
      default:
      {
	printf("error: unhandled option -%c\n", option);
	break;
      }
    }
  }

  for (option = ps.optind; option < argc; ++option)
  {
    printf("nonoption '%s'\n", argv[option]);
  }
}

void fillAudioBuffer(void *userdata, uint8_t *stream, int32_t len)
{
  //take care of configured sample type! here I assume it is AUDIO_S16
  int16_t* buffer = (int16_t*)stream;
  double val = 0;
  int i = 0;
  for(i=0; i < len/2; ++i)
  {
    double sample = (double)buffer[i];
    double amplitude = (sample < 0 ? sample / INT16_MIN : sample / INT16_MAX);
    double db =(amplitude == 0.0 ? 0 : 20*log10(amplitude));
    val += db;
  }
  val = val/(len/2);
  printf("Callback at %u, %d samples, %f\n", SDL_GetTicks(),len, val);
}

void init_audio_device(SDL_AudioDeviceID* dev, int32_t deviceId)
{
#ifdef NEW_SDL //need atleast sdl 2.0.16
  SDL_AudioSpec preferredAudioSpec;
  int32_t ret;
  ret = SDL_GetAudioDeviceSpec(deviceId, 1, &preferredAudioSpec);
  if(0 != ret)
  {
    // Could not get preferred audio spec
  }
#endif
  
  SDL_AudioSpec requestedAudioSpec;
  SDL_zero(requestedAudioSpec);
  requestedAudioSpec.freq = 44100;
  requestedAudioSpec.format = AUDIO_S16SYS;
  requestedAudioSpec.channels = 1;
  requestedAudioSpec.samples = 1024;
  requestedAudioSpec.callback = fillAudioBuffer;
  //requestedAudioSpec.userdata = gameData; //todo send in struct with format info etc
  
  SDL_AudioSpec actualAudioSpec;

  *dev = SDL_OpenAudioDevice(SDL_GetAudioDeviceName(deviceId, 1), 1,
			     &requestedAudioSpec,
			     &actualAudioSpec, 0);

  if (requestedAudioSpec.format != actualAudioSpec.format)
  {
    printf("We didn't get the wanted format.");
    return; //error code
  }
  if (*dev == 0)
  {
    printf("Failed to open audio: %s", SDL_GetError());
    return; //error code
  }
  return; //success
}

int main(int argc, char *argv[])
{
  if (SDL_Init(/*SDL_INIT_VIDEO |*/ SDL_INIT_AUDIO) != 0)
  {
    printf("Unable to initialize SDL: %s\n", SDL_GetError());
    return EXIT_FAILURE;
  }
  
  struct settings cmd_arg;
  parse_options(argc, argv, &cmd_arg);

  //Setup recording device
  print_selected_device_name(cmd_arg.selected_device_id);
  SDL_AudioDeviceID dev;
  init_audio_device(&dev, cmd_arg.selected_device_id);

  //Start sampling
  SDL_PauseAudioDevice(dev, 0);

  //Sample for some time
  // SDL_GetTicks64 give ms since start as Uint64, but need sdl 2.0.18 or newer, I have 2.0.8 as system version
  //printf("Started at %u\n", SDL_GetTicks());
  //SDL_Delay(5000);
  
  //Clean up
  SDL_CloseAudioDevice(dev);
  
  return EXIT_SUCCESS;
}
