import os
import thread
import time
import threading

class testthread(threading.Thread):
	def __init__(self,id):
		threading.Thread.__init__(self)
		self.id = id
	def run(self):
		while 1:
			time.sleep(1)
			print "hello I am thread %d" % self.id

def test_func():
	print "hello I am a test function"


print "hello I am the main function"
#print "now I'll sleep for 10 seconds"
#time.sleep(10)
#test_func()

thread1 = testthread(1)
thread1.start()

thread2 = testthread(2)
thread2.start()

thread1.join()
thread2.join()



