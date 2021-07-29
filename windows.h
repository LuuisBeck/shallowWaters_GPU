#ifndef PROJECT_WINDOWS_H
#define PROJECT_WINDOWS_H

class Windows
        {
        private:
            int height;
            int width;
            int grid_height;
int
        public:
            Windows(int height, int width, int day);

            void SetDate(int year, int month, int day);

            int getHeight() { return height; }
            int getWidth() { return width; }
            int getDay()  { return m_day; }
        };

#endif //PROJECT_WINDOWS_H
