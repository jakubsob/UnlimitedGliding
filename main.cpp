#include <GL/glut.h>
#include <cmath>
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <cstdio>
#include <vector>
#include <csignal>	
#include <fstream>
#include "objects.h"
#include "utils.h"

const int width = 1000;
const int height = 1000;
const char* print_format = "FPS=%4.2f, frame=%u, simulation count=%d, rho=%.2f";
const std::string YES = "y";
const std::string NO = "n";
const std::string READFILE = "f";
const char* FILENAME = "params.dat";

int reftime = 0,frame = 0, c_time = 0, timebase = 0;

unsigned long long frame_long = 0, simulation_threshold = 5000, simulation_length = 1000;
int simulations_count = 0, average_count = 1;

size_t size = 40, n_thermals = 400, n_drones = 100, n_measurments = 1;
float rho_thermals = n_thermals * std::pow(size,-2), increment_rho_thermals = 0.1, max_rho_thermals = 1., curr_rho_thernals = 0.;

const int max_range = 10;
const Objects::Color thermal_color = Objects::Color(1.,0.,0.), drone_color = Objects::Color(1.,1.,1.);

Objects::Rect** lattice;
std::vector<Objects::Cell> drones;
std::vector<Objects::Cell> thermals_list;
std::pair<bool,int> **thermals;

std::vector<float> velocity, average_velocity;
float sum_averagev = 0, sum_stddev = 0;

FILE* output_velocity;
int window_id;	

bool is_single = false, is_interaction = false, break_loop = false, is_visualize = false, is_write_to_file = false;

volatile sig_atomic_t flag = 0;
void close(int sig)
{
    flag = 1;
}

void init()
{
	signal(SIGINT, close); 
	srand48(time(NULL));
	float rect_size = 1./size;
	lattice = new Objects::Rect*[size];
	thermals = new std::pair<bool,int>*[size];
	for(size_t x = 0; x < size; x++)
	{
		lattice[x] = new Objects::Rect[size];
		thermals[x] = new std::pair<bool,int>[size];
		for(size_t y = 0; y < size; y++)
		{
			lattice[x][y] = Objects::Rect(2*float(x)/size - 1.,2*float(y)/size - 1.,2*float(x)/size - 1. + 2*rect_size,2*float(y)/size - 1. + 2*rect_size);
			thermals[x][y] = std::pair<bool,int>(false,0);
		}
	}

	drones.resize(n_drones);
	velocity.resize(n_drones);	
	average_velocity.resize(simulation_length);
	if(is_write_to_file)
	{
		output_velocity = fopen("outputVelocity.dat","a+");
		fprintf(output_velocity, "\n");
	}
}

void initSimulation()
{
	for(size_t x = 0; x < size; x++)
	{
		for(size_t y = 0; y < size; y++)
		{
			thermals[x][y].first = false;
			thermals[x][y].second = 0;
		}
	}
	thermals_list.clear();
	for(size_t x = 0; x < size; x++)
	{
		for(size_t y = 0; y < size; y++)
		{
			if(drand48() < rho_thermals)
			{
				thermals_list.push_back(Objects::Cell(x,y));
				thermals[x][y].first = true;
			}
		}
	}
	if(thermals_list.empty())
		return;

	for(size_t i = 0; i < n_drones; i++)
	{
		Objects::Cell thermal = thermals_list[std::floor(drand48()*(thermals_list.size()-1))];
		drones[i] = thermal;
		thermals[thermal.x][thermal.y].second++;		
	}
}

void exit()
{
	if(!is_single)
		fclose(output_velocity);
	break_loop = true;
	if(is_visualize)
	{
		glutDestroyWindow(window_id);
		exit(0);
	}
}

void moveDrones()
{
	if(thermals_list.empty())
		return;

	int i = 0;
	float lambda = 1./3.65;
	for(auto &d : drones)
	{
		int dx = std::floor(drand48()*(float(max_range)/2+1));
		int dy = std::floor(drand48()*(float(max_range)/2+1));
		if(drand48() > std::exp(-lambda*std::max(dx,dy)))
			continue;
		
		int old_x = d.x;
		int old_y = d.y;
		d.x = Utils::mod(d.x + dx,size);
		d.y = Utils::mod(d.y + dy,size);
		if(!thermals[d.x][d.y].first)
		{
			if(is_interaction)
			{
				std::vector<std::pair<int,int> > can_go;
				can_go.reserve(max_range*max_range);
				// switch loops for different models
				// for(int x = -max_range + dx; x <= max_range - dx; x++)
				for(int x = -max_range/2; x <= max_range/2; x++)
				{
					// for(int y =	-max_range + dy; y <= max_range - dy; y++)
					for(int y =	-max_range/2; y <= max_range/2; y++)
					{
						int n_x = Utils::mod(d.x + x,size);
						int n_y = Utils::mod(d.y + y,size);
						if(thermals[n_x][n_y].second > 0)
							can_go.push_back(std::pair<int,int>(n_x,n_y));
					}
				}
				int go = std::floor(drand48()*(can_go.size()));
				d.x = can_go[go].first;
				d.y = can_go[go].second;
			}
			else
			{
				d.x = old_x;
				d.y = old_y;
			}
		}
		thermals[old_x][old_y].second--;	
		thermals[d.x][d.y].second++; 
		
		if(!is_single)
		{
			int vx = Utils::mod_diff(d.x,old_x,size);
			int vy = Utils::mod_diff(d.y,old_y,size);
			velocity[i] = std::sqrt(vx*vx + vy*vy);
			velocity[i] = vx;
			i++;
		}	
	}
}

void measure()
{
	if(is_single)
		return;

	if(frame_long > simulation_threshold)
	{
		average_velocity[frame_long - simulation_threshold] = Utils::mean(velocity);
	}

	if(frame_long == simulation_threshold + simulation_length)
	{
		float averagev = Utils::mean(average_velocity);
		float sum = 0;
		for(auto v : average_velocity)
			sum += (v - averagev)*(v - averagev);
		sum_stddev += std::sqrt(sum/simulation_length);
		sum_averagev += averagev;
		simulations_count++;
		frame_long = 0;
		initSimulation();
	}

	if(simulations_count == average_count)
	{
		sum_averagev /= simulations_count;
		sum_stddev /= simulations_count;
		printf("rho=%.3f, avgerageV=%.2f, stdev=%.2f\n", rho_thermals, sum_averagev, sum_stddev);
		if(is_write_to_file)
			fprintf(output_velocity, "%f %f %f\n", rho_thermals, sum_averagev, sum_stddev);
		sum_averagev = 0;
		sum_stddev = 0;
		simulations_count = 0;
		frame_long = 0;
		rho_thermals += increment_rho_thermals;
		if(rho_thermals >= max_rho_thermals)
			exit();
		initSimulation();
	}
}

void print(double x, double y, char *string)
{
	glPushAttrib(GL_CURRENT_BIT);
	glColor4f(1.f,1.f,1.f,1.f);
	glRasterPos2f(x,y);
	for (size_t i = 0; i < strlen(string); i++) 
		glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_10,string[i]);
	glPopAttrib();
}

void display(void) {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  	
  	glColor4f(thermal_color.r, thermal_color.g, thermal_color.b, 0.6f);
  	for(auto thermal : thermals_list)
  	{
  		int x = thermal.x;
  		int y = thermal.y;
  		glRectf(lattice[x][y].x1,lattice[x][y].y1,lattice[x][y].x2,lattice[x][y].y2);			
	}
  	glColor4f(drone_color.r, drone_color.g, drone_color.b, 0.2f);
	for(auto drone : drones)
  	{
  		int x = drone.x;
  		int y = drone.y;
  		glRectf(lattice[x][y].x1,lattice[x][y].y1,lattice[x][y].x2,lattice[x][y].y2);			
	}

	moveDrones();
	measure();

	frame++;
	frame_long++;
	c_time = glutGet(GLUT_ELAPSED_TIME);
	char buffer[100];
	sprintf(buffer,print_format,frame*1000.0/(c_time-timebase),frame_long,simulations_count,rho_thermals);
	if (c_time - timebase > 1000) 
	{
		timebase = c_time;
		frame = 0;
	}
    print(-1,-1,buffer);

	glutSwapBuffers();
}

void Timer(int iUnused)
{
	glutPostRedisplay();
	glutTimerFunc(reftime, Timer, 0);
}

void keyboard(int key, int x, int y)
{
	switch(key)
	{
  		case GLUT_KEY_UP:
  			reftime += 1;
			break;
      	case GLUT_KEY_DOWN:
			if(reftime == 0)
				break;
			reftime -= 1;
			break;
		default: 
			break;
	}
}

void keyboardCB(unsigned char key, int x, int y)
{
	switch(key)
	{
		case 27:
			exit();
			break;
	}
	glutPostRedisplay();
}

void loop()
{
	init();
	initSimulation();
	while(!break_loop)
	{
		moveDrones();
		measure();
		frame_long++;
		if(flag)
        {
            exit();
            break;
        }  
	}
}

void loopGlut(int argc, char **argv)
{
	init();
	initSimulation();
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DEPTH | GLUT_DOUBLE | GLUT_ALPHA);
	glutInitWindowSize(width,height);
	window_id = glutCreateWindow("Unlimited Gliding");
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glClearColor(0.f, 0.f, 0.f, 1.f);
	glutDisplayFunc(display);
	glutSpecialFunc(keyboard);
	glutKeyboardFunc(keyboardCB);
	Timer(0);   
	glutMainLoop();
}

void parseParamsFile(int argc, char** argv)
{
	if(argc != 2)
	{
		std::cout << "Usage: <params.dat>\n";
		return;
	}
	
	std::ifstream file(argv[1]);
	if(!file.is_open())
	{
		std::cout << "No params.dat file\n";
		return;
	}

	auto parseBool = [&](std::string var, bool &value)
	{
		if(var.compare(YES) == 0)
			value = true;
		else if(var.compare(NO) == 0)
			value = false;
		else
			std::cout << "Wrong value, running with default <y> \n";
	};

	auto getValue = [&](std::ifstream &file, const std::string &delimiter, std::string &line)
	{
		std::getline(file, line);	
		line = line.substr(line.find(delimiter)+1,line.size());
	};

	std::string line;	
	std::string delimiter = "=";
	getValue(file,delimiter,line);
	parseBool(line, is_single);
	getValue(file,delimiter,line);
	parseBool(line, is_visualize);
	getValue(file,delimiter,line);
	parseBool(line, is_interaction);
	getValue(file,delimiter,line);
	parseBool(line, is_write_to_file);
	
	getValue(file,delimiter,line);
	size = std::stof(line);
	getValue(file,delimiter,line);
	n_drones = std::stof(line);
	getValue(file,delimiter,line);
	rho_thermals = std::stof(line);
	getValue(file,delimiter,line);
	max_rho_thermals = std::stof(line);
	getValue(file,delimiter,line);
	n_measurments = std::stof(line);
	increment_rho_thermals = (max_rho_thermals - rho_thermals)/n_measurments;
	getValue(file,delimiter,line);
	average_count = std::stof(line);
	getValue(file,delimiter,line);
	simulation_threshold = std::stof(line);
	getValue(file,delimiter,line);
	simulation_length = std::stof(line);
	file.close();
}	

int main(int argc, char **argv)
{
	parseParamsFile(argc,argv);
	is_visualize ? loopGlut(argc,argv) : loop();
	return 0;
}