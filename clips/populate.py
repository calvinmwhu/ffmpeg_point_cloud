import shutil

for i in range(0,60):
	name_color_0 = "color0_%s.mpg" % i
	name_color_1 = "color1_%s.mpg" % i
	name_depth_0 = "depth0_%s.mpg" % i
	name_depth_1 = "depth1_%s.mpg" % i
	shutil.copyfile('color0.mpg', name_color_0)
	shutil.copyfile('color1.mpg', name_color_1)
	shutil.copyfile('depth0.mpg', name_depth_0)
	shutil.copyfile('depth1.mpg', name_depth_1)


