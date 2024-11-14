#include <iostream>
#include <cstdlib>
#include <ctime>
#include <stack>
#include <vector>
#include <mutex>
#include <fstream>
#include <thread>
#include <atomic>

using namespace std;

enum CellState { Close, Open };

class Cell 
{
public:
    int x;
    int y;
    CellState Left = Close;
    CellState Right = Close;
    CellState Top = Close;
    CellState Bottom = Close;
    bool Visited = false;
    int value; 
    std::mutex mutex;

    Cell() : value(-1) {}
    explicit Cell(int value) : value(value) {}
    Cell(Cell const& cell) : value(cell.value) {};

};

int width;
int height;
atomic<int> globalThreadID{1}; 

void GenerateMaze(vector<vector<Cell>>& maze)
{
    vector<vector<Cell>> labyrinth(width, vector<Cell>(height));

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            labyrinth[x][y].x = x;
            labyrinth[x][y].y = y;
            labyrinth[x][y].Visited = false;
        }
    }

    srand(time(NULL));
    int startX = rand() % width;
    int startY = rand() % height;

    labyrinth[startX][startY].Visited = true;

    stack<Cell*> path;
    path.push(&labyrinth[startX][startY]);

    while (!path.empty())
    {
        Cell* _cell = path.top();

        vector<Cell*> nextStep;
        if (_cell->x > 0 && !labyrinth[_cell->x - 1][_cell->y].Visited)
            nextStep.push_back(&labyrinth[_cell->x - 1][_cell->y]);
        if (_cell->x < width - 1 && !labyrinth[_cell->x + 1][_cell->y].Visited)
            nextStep.push_back(&labyrinth[_cell->x + 1][_cell->y]);
        if (_cell->y > 0 && !labyrinth[_cell->x][_cell->y - 1].Visited)
            nextStep.push_back(&labyrinth[_cell->x][_cell->y - 1]);
        if (_cell->y < height - 1 && !labyrinth[_cell->x][_cell->y + 1].Visited)
            nextStep.push_back(&labyrinth[_cell->x][_cell->y + 1]);

        if (!nextStep.empty())
        {
            Cell* next = nextStep[rand() % nextStep.size()];

            if (next->x != _cell->x)
            {
                if (_cell->x - next->x > 0)
                {
                    _cell->Left = Open;
                    next->Right = Open;
                }
                else
                {
                    _cell->Right = Open;
                    next->Left = Open;
                }
            }
            if (next->y != _cell->y)
            {
                if (_cell->y - next->y > 0)
                {
                    _cell->Top = Open;
                    next->Bottom = Open;
                }
                else
                {
                    _cell->Bottom = Open;
                    next->Top = Open;
                }
            }
            next->Visited = true;
            path.push(next);
        }
        else
        {
            path.pop();
        }
    }

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            int gridX = x * 2 + 1;
            int gridY = y * 2 + 1;

            maze[gridY][gridX].value = 0;

            if (labyrinth[x][y].Right == Open)
            {
                maze[gridY][gridX + 1].value = 0;
            }
            if (labyrinth[x][y].Bottom == Open)
            {
                maze[gridY + 1][gridX].value = 0;
            }
        }
    }
}

void Print(const vector<vector<Cell>>& consoleGrid)
{
    int outputWidth = width * 2 + 1;
    int outputHeight = height * 2 + 1;

    for (int y = 0; y < outputHeight; ++y) 
    {
        for (int x = 0; x < outputWidth; ++x)
        {
            if (consoleGrid[y][x].value == -1)
                cout << "#";  
            else if (consoleGrid[y][x].value == 0)
                cout << " ";  
            else
                cout << consoleGrid[y][x].value;
        }
        cout << endl;
    }
}

void TraverseMaze(int tid, vector<vector<Cell>>& maze, int x, int y)
{
    vector<thread> childThreads;
    
    while (true)
    {
        vector<pair<int, int>> possibleMoves;

        
        {
            lock_guard<mutex> lock(maze[y][x].mutex);
            maze[y][x].value = tid;
        }

        
        if (y > 0 && maze[y - 1][x].value == 0)
        {
            lock_guard<mutex> lock(maze[y - 1][x].mutex);
            if (maze[y - 1][x].value == 0) 
                possibleMoves.emplace_back(x, y - 1); 
        }
        if (x < width * 2 && maze[y][x + 1].value == 0)
        {
            lock_guard<mutex> lock(maze[y][x + 1].mutex);
            if (maze[y][x + 1].value == 0) 
                possibleMoves.emplace_back(x + 1, y); 
        }
        if (y < height * 2 && maze[y + 1][x].value == 0)
        {
            lock_guard<mutex> lock(maze[y + 1][x].mutex);
            if (maze[y + 1][x].value == 0) 
                possibleMoves.emplace_back(x, y + 1); 
        }
        if (x > 0 && maze[y][x - 1].value == 0)
        {
            lock_guard<mutex> lock(maze[y][x - 1].mutex);
            if (maze[y][x - 1].value == 0) 
                possibleMoves.emplace_back(x - 1, y); 
        }

        
        if (possibleMoves.empty())
        {
            break;
        }
        else if (possibleMoves.size() == 1) 
        {
            
            x = possibleMoves[0].first;
            y = possibleMoves[0].second;
        }
        else 
        {
            
            for (size_t i = 1; i < possibleMoves.size(); ++i) 
            {
                int newX = possibleMoves[i].first;
                int newY = possibleMoves[i].second;
                int newThreadID = globalThreadID++;
                childThreads.emplace_back(TraverseMaze, newThreadID, ref(maze), newX, newY);
            }

            
            x = possibleMoves[0].first;
            y = possibleMoves[0].second;
        }
    }

    
    for (auto& child : childThreads)
    {
        if (child.joinable())
            child.join();
    }
}


void SaveToPPM(const vector<vector<Cell>>& maze, const string& filename)
{
    int scale = 100;  
    int outputWidth = maze[0].size() * scale;
    int outputHeight = maze.size() * scale;
    
    ofstream file(filename, ios::binary);
    file << "P6\n" << outputWidth << " " << outputHeight << "\n255\n";

    for (int y = 0; y < maze.size(); ++y) 
    {
        for (int dy = 0; dy < scale; ++dy) 
        {
            for (int x = 0; x < maze[0].size(); ++x) 
            {
                for (int dx = 0; dx < scale; ++dx) 
                {
                    char r, g, b;
                    if (maze[y][x].value == -1) {
                        r = g = b = 255; 
                    } else if (maze[y][x].value == 0) {
                        r = g = b = 0; 
                    } else {
                        r = (maze[y][x].value * 50) % 255;
                        g = (maze[y][x].value * 80) % 255;
                        b = (maze[y][x].value * 100) % 255;
                    }
                    file << r << g << b;
                }
            }
        }
    }
    file.close();
}



int main() 
{
    cout << "Enter the width of the maze: ";
    cin >> width;
    cout << "Enter the height of the maze: ";
    cin >> height;

    int outputWidth = width * 2 + 1;
    int outputHeight = height * 2 + 1;
    vector<vector<Cell>> maze(outputHeight, vector<Cell>(outputWidth));
    
    GenerateMaze(maze);
    //Print(maze);
    
    int startX , startY;
    while(true)
    {
       startX = rand() % (width * 2) + 1; 
       startY = rand() % (height * 2) + 1;
       if(maze[startY][startX].value == 0)
          break;
    }
    thread threads{TraverseMaze, globalThreadID++, ref(maze),startX, startY};

    if(threads.joinable())
       threads.join();

    //Print(maze);
    
    cout << startX << " " << startY;

    SaveToPPM(maze,"Maze.ppm");

    return 0;
}

