#include<gl/glut.h>
#include<GL/freeglut_ext.h>
#include<string>
#include<math.h>
#include<time.h>
#include<stdio.h>
#include<iostream>
using namespace std;
#define DISTANCE 9999 //最大视距
#define random(x) (rand()%x)//产生0~x-1之间的随机数
#define BMP_Header_Length 54  //图像数据在内存块中的偏移量
const float pi = 3.1415926f;

float WinLong = 1980 / 2 - 100;//视窗长宽z
float WinWide = 1080 / 2 + 200;
float camera_x, camera_y, camera_z;//相机移动位置
float univer_r;//空间旋转

int level;//迭代次数
int lineState;//是否绘制网线，为-1不绘制，1绘制
int veinState;//是否加载纹理
int projectWay;//投影方式
GLuint tree, leaf;//纹理
//其本地bmp文件需要添加在工程项目中
//debug生成的exe需要与bmp存在于同一文件夹才能加载

//判断一个整数是不是2的整数次幂
int power_of_two(int n)
{
	if (n <= 0) {
		return 0;
	}
	return (n & (n - 1)) == 0;
}
//读取bmp文件加载纹理并返回
GLuint LoadTexture(const char* file_name)
{
	GLint width, height, total_bytes;
	GLubyte* pixels = 0;
	GLuint last_texture_ID = 0, texture_ID = 0;

	// 打开文件，如果失败，返回	
	FILE* pFile;
	if (fopen_s(&pFile, file_name, "rb")) {
		//printf_s("文件 %s 加载失败 无法打开\n", file_name);
		return 0;
	}

	// 读取文件中图象的宽度和高度	
	fseek(pFile, 0x0012, SEEK_SET);
	fread(&width, 4, 1, pFile);
	fread(&height, 4, 1, pFile);
	fseek(pFile, BMP_Header_Length, SEEK_SET);

	// 计算每行像素所占字节数，并根据此数据计算总像素字节数	
	GLint line_bytes = width * 3;
	while (line_bytes % 4 != 0) {
		++line_bytes;
	}
	total_bytes = line_bytes * height;

	// 根据总像素字节数分配内存
	pixels = (GLubyte*)malloc(total_bytes);
	if (0 == pixels) {
		fclose(pFile);
		printf_s("文件 %s 加载失败 像素为0\n", file_name);
		return 0;
	}

	// 读取像素数据	
	if (fread(pixels, total_bytes, 1, pFile) <= 0) {
		free(pixels);
		fclose(pFile);
		printf_s("文件 %s 加载失败 数据缺失\n", file_name);
		return 0;
	}

	// 对就旧版本的兼容，如果图象的宽度和高度不是的整数次方，则需要进行缩放
	// 若图像宽高超过了OpenGL规定的最大值，也缩放	
	GLint max;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max);
	if (!power_of_two(width)
		|| !power_of_two(height)
		|| width > max
		|| height > max)
	{
		const GLint new_width = 256;
		const GLint new_height = 256; // 规定缩放后新的大小为边长的正方形		
		GLint new_line_bytes, new_total_bytes;
		GLubyte* new_pixels = 0;

		// 计算每行需要的字节数和总字节数	
		new_line_bytes = new_width * 3;
		while (new_line_bytes % 4 != 0) {
			++new_line_bytes;
		}
		new_total_bytes = new_line_bytes * new_height;

		// 分配内存		
		new_pixels = (GLubyte*)malloc(new_total_bytes);
		if (0 == new_pixels) {
			free(pixels);
			fclose(pFile);
			printf_s("文件 %s 加载失败 内存分配失败\n", file_name);
			return 0;
		}

		// 进行像素缩放		
		gluScaleImage(GL_RGB,
			width, height, GL_UNSIGNED_BYTE, pixels,
			new_width, new_height, GL_UNSIGNED_BYTE, new_pixels);

		// 释放原来的像素数据，把pixels指向新的像素数据，并重新设置width和height		
		free(pixels);
		pixels = new_pixels;
		width = new_width;
		height = new_height;
	}

	//设置白色为透明的0,之前读取的RGB，现在需要RGBA
	unsigned char* texture = 0;
	texture = (unsigned char*)malloc(width * height * 4);
	for (int m = 0; m < width; m++) {
		for (int j = 0; j < height; j++) {
			//把颜色值写入   	
			texture[m*width * 4 + j * 4] = pixels[m*width * 3 + j * 3];
			texture[m*width * 4 + j * 4 + 1] = pixels[m*width * 3 + j * 3 + 1];
			texture[m*width * 4 + j * 4 + 2] = pixels[m*width * 3 + j * 3 + 2];

			//设置alpha值,假设白色为透明色   
			if (texture[m*width * 4 + j * 4] >= 200
				&& texture[m*width * 4 + j * 4 + 1] >= 200
				&& texture[m*width * 4 + j * 4 + 2] >= 200) {
				texture[m*width * 4 + j * 4 + 3] = 0;//透明，alpha=0  
				//cout << (int)texture[m*width * 4 + j * 4 + 3];
			}
			else {
				texture[m*width * 4 + j * 4 + 3] = 255;//不透明，alpha=255  
				//cout << (int)texture[m*width * 4 + j * 4 + 3];
			}
		}
	}

	// 分配一个新的纹理编号
	glGenTextures(1, &texture_ID);
	if (0 == texture_ID) {
		free(pixels);
		free(texture);
		fclose(pFile);
		printf_s("文件 %s 加载失败 纹理分配失败\n", file_name);
		return 0;
	}

	// 绑定新的纹理，载入纹理并设置纹理参数
	// 在绑定前，先获得原来绑定的纹理编号，以便在最后进行恢复	
	GLint lastTextureID = last_texture_ID;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &lastTextureID);
	glBindTexture(GL_TEXTURE_2D, texture_ID);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glTexImage2D(GL_TEXTURE_2D, 0, 4, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture);
	gluBuild2DMipmaps(GL_TEXTURE_2D, 4, width, height, GL_BGRA_EXT, GL_UNSIGNED_BYTE, texture);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	glBindTexture(GL_TEXTURE_2D, lastTextureID);  //恢复之前的纹理绑定
	free(pixels);
	free(texture);
	//printf_s("文件 %s 纹理加载 成功\n", file_name);
	return texture_ID;
}

//文法生成元
//F绘制树干，X绘制叶子，AB更改树干参数，+-/*%&坐标变换，[]进出栈
string grammar1 = "FA[+*&X][-/X][+%X]B";
string grammar2 = "FA[%-X][&*X][+/X][-*X]B";
string grammar3 = "FA[-*X][/&X][*%X][+&X]B";
string treegra;

//根据元生成文法字符串
string grammarIteration(string grammar, int level)
{
	string tempRule = grammar;
	string rule = {};
	for (int i = 1; i <= level; i++)
	{
		int curlen = tempRule.length();
		int j = 0;
		srand((int)time(0));//随机种子
		while (j < curlen) {
			if ('X' == tempRule[j]) { // X迭代，替换成文法模型		
				int n = random(3);
				//cout << n << endl;
				if (0 == n) {
					rule += grammar3;
				}
				else if (1 == n) {
					rule += grammar2;
				}
				else if (2 == n) {
					rule += grammar1;
				}
				j++;
			}
			else { // 保留	
				rule += tempRule[j];
				j++;
			}
		}
		tempRule = rule;
		rule.clear();
	}
	rule = tempRule;
	return rule;
}

// 绘制树干
void drawTree(float baseRadius, float topRadius, float height)
{
	GLUquadricObj *tree_obj;
	tree_obj = gluNewQuadric();//创建二次曲面对象
	if (tree != 0 && veinState == 1) {
		glBindTexture(GL_TEXTURE_2D, tree);//绑定纹理
		gluQuadricTexture(tree_obj, GL_TRUE);
	}
	else {
		glColor3ub(155, 81, 33);//纹理加载失败时代替
	}
	gluCylinder(tree_obj, baseRadius, topRadius, height, 30, 30);//绘制
	glTranslatef(0.0f, 0.0f, height);
	gluSphere(tree_obj, topRadius, 30, 30);
	gluDeleteQuadric(tree_obj);
}
// 绘制叶子
void drawLeaf()
{
	if (leaf != 0 && veinState == 1) {
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, 0.9f);
		glBindTexture(GL_TEXTURE_2D, leaf);//绑定纹理
		glBegin(GL_QUADS);
		glTexCoord2f(0.0, 0.0); glVertex3f(0, 0, 0);
		glTexCoord2f(1.0, 0.0); glVertex3f(10, 10, 0);
		glTexCoord2f(1.0, 1.0); glVertex3f(0, 20, 0);
		glTexCoord2f(0.0, 1.0); glVertex3f(-10, 10, 0);
		glEnd();
		glDisable(GL_ALPHA_TEST);
	}
	else {
		glColor3f(1.0f, 0.0f, 0.0f);//纹理加载失败时代替
		glBegin(GL_QUADS);
		glVertex3f(0, 0, 0);
		glVertex3f(5, 5, 0);
		glVertex3f(0, 10, 0);
		glVertex3f(-5, 5, 0);
		glEnd();
	}
}
// 根据文法绘制分形树
void grammarDraw(string grammar)
{
	float baseRadius = 10, topRadius = 6, height = 200;
	int i = 0;
	while (i < grammar.length()) {
		switch (grammar[i]) {
			//F绘制树干，X绘制叶子，AB更改树干参数，+-/*%&坐标变换，[]进出栈
		case'F':drawTree(baseRadius, topRadius, height); break;
		case'X':drawLeaf(); break;
		case'A':height *= 0.6; baseRadius *= 0.5; topRadius *= 0.5; break;
		case'B':height /= 0.6; baseRadius /= 0.5; topRadius /= 0.5; break;
		case'+':glRotatef(35, 1, 0, 0); break;
		case'-':glRotatef(-35, 1, 0, 0); break;
		case'*':glRotatef(35, 0, 1, 0); break;
		case'/':glRotatef(-35, 0, 1, 0); break;
		case'%':glRotatef(35, 0, 0, 1); break;
		case'&':glRotatef(-35, 0, 0, 1); break;
		case'[':glPushMatrix(); break;
		case']':glPopMatrix(); break;
		}
		i++;
	}
}

// 绘制空间（Pojectway=-1使用正投影，否则使用透视投影）
void DrawRoom(int PojectWay)
{
	glMatrixMode(GL_PROJECTION);//投影矩阵应用
	glLoadIdentity();
	//设置投影空间
	if (-1 == PojectWay) {
		//正投影空间
		glOrtho(-WinLong / 2, WinLong / 2, -WinWide / 2, WinWide / 2, -DISTANCE, DISTANCE);
	}
	else {
		//透视投影空间
		gluPerspective(70, WinLong / WinWide, 1, DISTANCE / 4);//视野最远距离distance/6，不然远处像素堆积在一起了
	}

	glMatrixMode(GL_MODELVIEW);	//模型视图矩阵应用
	glLoadIdentity();

	//设定相机位置
	glTranslatef(camera_x, camera_y, camera_z);//移动位置
	gluLookAt(WinWide / 2, WinWide / 2, WinWide / 4, 0, 0, 0, 0, 0, 1);//设定机位
	glRotatef(univer_r, 0, 0, 1);//设定机位后旋转为旋转空间

	//绘制网面
	if (1 == lineState) {
		glBegin(GL_LINES);
		glColor3f(1, 0, 0);//红色x轴
		glVertex3f(-DISTANCE, 0, 0); glVertex3f(DISTANCE, 0, 0);

		glColor3f(0, 1, 0);//绿色y轴
		glVertex3f(0, -DISTANCE, 0); glVertex3f(0, DISTANCE, 0);

		glColor3f(0, 0.7, 1);//蓝色z轴
		glVertex3f(0, 0, -DISTANCE); glVertex3f(0, 0, DISTANCE);

		glColor3f(0.5, 0.5, 0.5);
		for (int i = 0; i * 50 <= DISTANCE; i++) {
			glVertex2f(i * 50, -DISTANCE); glVertex2f(i * 50, DISTANCE);
			glVertex2f(-i * 50, -DISTANCE); glVertex2f(-i * 50, DISTANCE);
		}
		for (int i = 0; i * 50 <= DISTANCE; i++) {
			glVertex2f(-DISTANCE, i * 50); glVertex2f(DISTANCE, i * 50);
			glVertex2f(-DISTANCE, -i * 50); glVertex2f(DISTANCE, -i * 50);
		}
		glEnd();
	}

	//光源
	glEnable(GL_LIGHTING);//启用灯源
	GLfloat light_position[] = { 100, 100, 100, 1.0 };//w=1.0，定位光源
	GLfloat light_ambient[] = { 0.0, 0.0, 0.0, 1.0 };
	GLfloat light_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
	GLfloat light_specular[] = { 1.0, 1.0, 1.0, 1.0 };
	glLightfv(GL_LIGHT0, GL_POSITION, light_position);
	glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
	glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
	glEnable(GL_LIGHT0);//打开灯光1

	if (veinState == 1) {
		glEnable(GL_TEXTURE_2D);//纹理功能
	}

	// 绘制分形树
	grammarDraw(treegra);

	glLoadIdentity();
	if (veinState == 1) {
		glDisable(GL_TEXTURE_2D);//关闭纹理
	}
	glDisable(GL_LIGHTING);//关闭灯源
}
// 显示函数
void myDisplay(void)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);//颜色和深度缓存清空

	//绘制左图像
	glViewport(0, 0, WinLong, WinWide);
	DrawRoom(projectWay);

	glutSwapBuffers();   //双缓冲刷新
}
// 初始化函数 
void Initial(void)
{
	//参数初始化
	int starGrammar = random(3);
	if (0 == starGrammar) {
		treegra = grammarIteration(grammar1, level);
	}
	else if (1 == starGrammar) {
		treegra = grammarIteration(grammar2, level);
	}
	else if (2 == starGrammar) {
		treegra = grammarIteration(grammar3, level);
	}
	cout << "文法为：" << treegra << endl << endl;

	glEnable(GL_COLOR_MATERIAL);//光照下希望模型的颜色可以起作用
	glEnable(GL_DEPTH_TEST);//启动深度测试，GL绘制是按代码顺序绘制的，启动深度则出现遮挡关系
	glDepthFunc(GL_LESS);//现实生活中的前景

	glClearColor(0.8f, 0.8f, 0.8f, 0.1f);//背景，前3个是RGB，最后是Alpha值，控制透明，0.0表示完全透明
	//绘制函数中分区按不同投影方式绘制，不在初始化中申请投影空间
	
	leaf = LoadTexture("leaf.bmp");
	tree = LoadTexture("tree.bmp");//读取bmp文件加载纹理
}

// 键盘回调函数
void keyBoard(unsigned char key, int x, int y) {
	switch (key) {
	case 'q':
		camera_x = 0;
		camera_y = -200;
		camera_z = 0;
		univer_r = 0;
		break;
	case 'e':projectWay *= -1; break;
		//从相机的xyz轴移动世界坐标，相对效果则相当于相机在世界坐标中 反 方向移动
	case 'w':camera_z += 10; break;
	case 's':camera_z -= 10; break;
	case 'a':camera_x += 10; break;
	case 'd':camera_x -= 10; break;

	case 'z':univer_r += 1; break;
	case 'c':univer_r -= 1; break;

	case '1':level = 5; cout << "迭代次数：" << level << endl; Initial(); break;
	case '2':lineState *= -1; break;
	case '3':veinState *= -1; break;

	case'4':level--; cout << "迭代次数：" << level << endl; Initial(); break;
	case'5':level++; cout << "迭代次数：" << level << endl; Initial(); break;
	}
	glutPostRedisplay();  //重画，重新调用Display()
}
// 鼠标回调
void onMouseWheel(int button, int dir, int x, int y)
{
	if (dir > 0) {
		camera_y -= 7.5;
	}
	if (dir < 0) {
		camera_y += 7.5;
	}
	glutPostRedisplay();  //重画，重新调用Display()
}

// 使用glut库函数进行窗口管理
int main(int argc, char * argv[])
{
	//参数初始化
	camera_x = 0;
	camera_y = -200;
	camera_z = 0;
	lineState = 1;
	veinState = 1;
	projectWay = -1;
	level = 5;

	printf_s("操作说明：请关闭大写输入，tree/leaf.bmp文件与该exe放于同一目录下，不放也没事 ");
	printf_s("\n\n迭代次数不建议 >= 8\n");
	printf_s("\n     1 - 重新生成5迭代树     2 - 打开网线    3 - 打开纹理         4 - 减少迭代次数     5 - 增加迭代次数");
	printf_s("\n     q - 返回初始机位                   e - 切换投影方式  ");
	printf_s("\n                        w - 前移                          ");
	printf_s("\n     a - 左移           s - 后移        d - 右移          ");
	printf_s("\n     z - 空间顺时旋转                   c - 空间逆时旋转  ");
	printf_s("\n                        x - 关闭/打开网线                 ");
	printf_s("\n    鼠标滚轮向上滚动 - 上移            鼠标滚轮向下滚动 - 下移\n\n");

	//窗口初始化
	glutInit(&argc, argv);//glut初始化
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);//RGBA颜色模型|双缓冲
	glutInitWindowPosition(0, 100);//初始位置
	glutInitWindowSize(WinLong, WinWide);//窗口大小

	glutCreateWindow("杨博文—L分形树");//创建窗口

	Initial();//初始化
	glutDisplayFunc(&myDisplay);//myDisplay指定为当前窗口的显示内容函数

	//注册回调函数
	glutKeyboardFunc(&keyBoard);
	glutMouseWheelFunc(&onMouseWheel);
	glutMainLoop();//使窗口框架运行

	return 0;
}
