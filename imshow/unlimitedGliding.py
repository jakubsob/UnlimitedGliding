#!bin/usr
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
import time

class Simulation:
	def __init__(self, n, n_therm, n_drones):
		self.n = n
		self.n_therm = n_therm
		self.n_drones = n_drones
		self.rho = float(n_therm) / n ** 2 
		self.thermals = []
		self.known_thermals = []
		self.drones = []
		self.max_move = 5
		self.base_img = np.zeros((self.n,self.n,4),dtype=np.uint8)
		self.base_img[:,:,3] = 255
		self.drones_img = np.zeros((self.n,self.n,4),dtype=np.uint8)
		self.known_thermals_plane = np.zeros((self.n,self.n),dtype=np.uint64)
		
	def init_all(self):
		self.init_thermals()
		self.init_drones()

	def init_thermals(self):
		self.thermals = [[y,x] for y in range(self.n) for x in range(self.n) if np.random.random() < self.rho]
		self.thermals = np.asarray(self.thermals)
		self.base_img[tuple(self.thermals.T)] = [255,0,0,255]
		
	def init_drones(self):
		randint = np.random.randint(low=0,high=len(self.thermals),size=self.n_drones)
		self.drones = [[self.thermals[randint[d]][0], self.thermals[randint[d]][1]] for d in range(self.n_drones)]
		self.drones = np.asarray(self.drones)
		self.known_thermals = self.drones
		
		self.drones_img[tuple(self.known_thermals.T)] = [0,255,0,0]
		self.drones_img[tuple(self.drones.T)] = [0,0,255,255]
		for drone in self.drones:
			self.known_thermals_plane[drone[0],drone[1]] += 1

	def move_drones(self): 
		dX = np.random.randint(low=0, high=self.max_move + 1,size=self.n_drones)
		dY = np.random.randint(low=0, high=self.max_move + 1,size=self.n_drones)
		uniform = np.random.random(size=self.n_drones)
		for d in range(self.n_drones):
			if not uniform[d] < np.exp(-max(dX[d],dY[d])):	
				continue
			y, x = self.drones[d][0], self.drones[d][1]
			n_y, n_x = (self.drones[d][0] + dY[d]) % self.n, (self.drones[d][1] + dX[d]) % self.n
			self.drones[d][0] = n_y
			self.drones[d][1] = n_x
			if self.base_img[n_y,n_x][0] == 255:
				self.known_thermals_plane[n_y,n_x] += 1
			else:
				can_go = [[(self.drones[d][0]+dy)%self.n,(self.drones[d][1]+dx)%self.n] for dx in range(-dX[d],dX[d]+1) for dy in range(-dY[d],dY[d]+1) if self.known_thermals_plane[(self.drones[d][0]+dy)%self.n,(self.drones[d][1]+dx)%self.n] > 0]
				thermal = can_go[np.random.randint(0,len(can_go))]
				n_y, n_x = thermal[0], thermal[1]
				self.drones[d][0] = n_y			
				self.drones[d][1] = n_x
				self.known_thermals_plane[n_y,n_x] += 1
			self.known_thermals_plane[y,x] -= 1
						
	def loop(self):
		fig = plt.figure(figsize=(12,12))
		ax = fig.gca()
		plt.tight_layout()
		base = plt.imshow(self.base_img,animated=True)
		self.im = plt.imshow(self.base_img,animated=True)
		
		self.time_text = ax.text(0, 0, '', transform=ax.transAxes, color='w')
		self.iterate = 0

		self.start = time.time()
		_ = FuncAnimation(fig, self.update, interval=0, blit=True)
		plt.show()
			
	def update(self,frame_number):
		self.drones_img = np.zeros(self.base_img.shape,dtype=np.uint8)
		self.drones_img[tuple(self.drones.T)] = (0,0,255,255)
		self.im.set_data(self.drones_img + self.base_img)
		self.move_drones()
		self.iterate = self.iterate + 1
		self.time_text.set_text("fps={:f}".format(self.iterate/(time.time()-self.start)))
		return [self.im, self.time_text]
	
if __name__ == '__main__':
	n = 40	
	n_therm = 400
	n_drones = 100
	simulation = Simulation(n, n_therm, n_drones)
	simulation.init_all()
	simulation.loop()