#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "parg.h"
#include "SDL.h"

//----------------- Structs -----------------------------
struct settings
{
  int32_t selected_device_id;
  SDL_AudioSpec actualAudioSpec;

  bool enable_visualization;
};

struct state
{
  struct settings opt;

  SDL_Window* window;
  SDL_Renderer* renderer;
  int32_t window_width;
  int32_t window_height;
  int32_t draw_col;
  double* col_data;
};
//-------------------------------------------------------

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

void parse_options(int argc, char* argv[],
		   struct settings* cmd_arg)
{
  //default init cmd_arg
  cmd_arg->selected_device_id = 0;
  cmd_arg->enable_visualization = false;
  
  // Parse command line args
  struct parg_state ps;
  int32_t option; 
  parg_init(&ps);

  while((option = parg_getopt(&ps, argc, argv, "hs:vld:r")) != -1)
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
      case 'r':
      {
	cmd_arg->enable_visualization = true;
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
  struct state* prog_state = (struct state*)userdata;
  struct settings* opt = &prog_state->opt;
  SDL_AudioSpec* audio_spec = &opt->actualAudioSpec;
  
  assert(AUDIO_S16SYS == audio_spec->format);

  int32_t  sample_freq  = audio_spec->freq;
  uint16_t sample_count = audio_spec->samples;
  int16_t* samples = (int16_t*)stream;

  //Downsample to something more reasonable for our application
  //moving average filter and then decimate
  //good signal in time domain, but bad in frequency!
  //if we need to do dft, use windowed sinc-filter instead

  //world record is somewhere around 1200 bpm, 20 Hz
  int32_t target_freq = 1000;
  
  double duration_ms = ((double)sample_count / sample_freq)*1000;

  int32_t distance_between_samples = sample_freq/target_freq;
  int32_t first_sample_ix = distance_between_samples/2;

  int32_t window_size = 11 < distance_between_samples ? 11 : distance_between_samples;
  int32_t half_window = (window_size - 1)/2;

  double amp_buf[1024];
  size_t buf_ix = 0;
  
  int32_t i = first_sample_ix - half_window;
  int32_t j;
  for(;
      i + window_size < sample_count;
      i += distance_between_samples)
  {
    amp_buf[buf_ix] = 0;
    for(j = i; j < i + window_size; ++j)
    {
      double sample = (double)samples[j];
      double amplitude = (sample < 0 ? sample / INT16_MIN : sample / INT16_MAX);
      amp_buf[buf_ix] += amplitude;
    }
    amp_buf[buf_ix] /= window_size;
    ++buf_ix;
    //double db = 20*log10(amplitude); //how to handle amplitude == 0? set amplitude to 1/INT16_MAX ?

  }
  printf("Callback at %u, %d samples\n", SDL_GetTicks(),sample_count);

  //draw stuff
  if(opt->enable_visualization)
  {
    SDL_SetRenderDrawColor(prog_state->renderer, 0xFF, 0xFF, 0xFF, 0xFF);
    for(i = 0; i < buf_ix; ++i)
    {
      if(prog_state->draw_col >= prog_state->window_width)
      {
	SDL_SetRenderDrawColor(prog_state->renderer, 0x00, 0x00, 0x00, 0x00);
	SDL_RenderClear(prog_state->renderer);
	SDL_SetRenderDrawColor(prog_state->renderer, 0xFF, 0xFF, 0xFF, 0xFF);
	prog_state->draw_col = 0;
      }
      int32_t ix = prog_state->draw_col;

      prog_state->col_data[ix] = amp_buf[i];
      int16_t draw_value = (int16_t)(amp_buf[i]*prog_state->window_height);

      if(ix == 0)
      {
	SDL_RenderDrawPoint(prog_state->renderer,ix , draw_value);
      }
      else
      {
	int16_t y1 = (int16_t)(prog_state->col_data[ix-1]*prog_state->window_height);
	  
	SDL_RenderDrawLine(prog_state->renderer, ix-1, y1, ix, draw_value);
      }
      prog_state->draw_col += 1;
    }
    SDL_RenderPresent(prog_state->renderer);
  }

}

void init_audio_device(SDL_AudioDeviceID* dev, struct state* prog_state)
{
  struct settings* opt = &prog_state->opt;
  int32_t device_id = opt->selected_device_id;
  
#ifdef NEW_SDL //need atleast sdl 2.0.16
  SDL_AudioSpec preferredAudioSpec;
  int32_t ret;
  ret = SDL_GetAudioDeviceSpec(device_id, 1, &preferredAudioSpec);
  if(0 != ret)
  {
    // Could not get preferred audio spec
  }
#endif
  
  SDL_AudioSpec requestedAudioSpec;
  SDL_zero(requestedAudioSpec);
  requestedAudioSpec.freq = 48000;
  requestedAudioSpec.format = AUDIO_S16SYS;
  requestedAudioSpec.channels = 1;
  requestedAudioSpec.samples = 1024;
  requestedAudioSpec.callback = fillAudioBuffer;
  requestedAudioSpec.userdata = prog_state;
  
  *dev = SDL_OpenAudioDevice(SDL_GetAudioDeviceName(device_id, 1), 1,
			     &requestedAudioSpec,
			     &opt->actualAudioSpec, 0);

  if (requestedAudioSpec.format != opt->actualAudioSpec.format)
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
  if (SDL_Init(SDL_INIT_AUDIO) != 0)
  {
    printf("Unable to initialize SDL: %s\n", SDL_GetError());
    return EXIT_FAILURE;
  }

  struct state prog_state = {0};
  parse_options(argc, argv, &prog_state.opt);

  //Setup recording device
  print_selected_device_name(prog_state.opt.selected_device_id);
  SDL_AudioDeviceID dev;
  init_audio_device(&dev, &prog_state);

  //Setup visualization if enabled
  if(prog_state.opt.enable_visualization)
  {
    //The window we'll be rendering to
    prog_state.window = NULL;
    
    //The surface contained by the window
    SDL_Surface* screenSurface = NULL;

    //Initialize SDL
    if( SDL_Init( SDL_INIT_VIDEO ) < 0 )
    {
      printf( "SDL could not initialize! SDL_Error: %s\n", SDL_GetError() );
    }
    else
    {
      prog_state.draw_col = 0;
      prog_state.window_width  = 1000;
      prog_state.window_height = 255;
      prog_state.col_data = (double*)malloc(sizeof(double)* prog_state.window_width);
      const int32_t width  = prog_state.window_width;
      const int32_t height = prog_state.window_height;
      //Create window and renderer
      SDL_CreateWindowAndRenderer(width, height, 0,
				  &prog_state.window,
				  &prog_state.renderer);
      if( prog_state.window == NULL )
      {
	printf( "Window could not be created! SDL_Error: %s\n", SDL_GetError() );
      }
    }
  }//if enable_visualization
  
  //Start sampling
  SDL_PauseAudioDevice(dev, 0);

  //Sample for some time
  // SDL_GetTicks64 give ms since start as Uint64, but need sdl 2.0.18 or newer, I have 2.0.8 as system version
  printf("Started at %u\n", SDL_GetTicks());
  SDL_Delay(5000);
  SDL_Delay(5000);
  
  //Clean up
  SDL_CloseAudioDevice(dev);
  if(prog_state.opt.enable_visualization)
  {
    SDL_DestroyWindow( prog_state.window );
    SDL_DestroyRenderer(prog_state.renderer);
    if(prog_state.col_data != NULL)
    {
      free(prog_state.col_data);
    }
  }
  SDL_Quit();
  
  return EXIT_SUCCESS;
}
