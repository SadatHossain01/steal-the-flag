#pragma GCC optimize("O3,inline")
#pragma GCC target("bmi,bmi2,lzcnt,popcnt")

#include <algorithm>
#include <bitset>
#include <cassert>
#include <iostream>
#include <numeric>
#include <queue>
#include <set>
#include <string>
#include <vector>
using namespace std;

typedef long long ll;

struct Powerup;
struct Game;

int height, width, n_minions = -1;
const int INF = 5000;
bool moveDone;          // make sure no minion move is wasted?
bool firstTime = true;  // for last state of the game
vector<string> grid;
vector<pair<int, int>> not_visited;
set<int> blocked;
set<int> explorerMinions;
int myFlagBaseX, myFlagBaseY, oppFlagBaseX, oppFlagBaseY;
// this is for different calls of bfs()
vector<vector<bool>> visited;
vector<int> assignedExploration;
vector<vector<pair<int, int>>> cellsToExplore;
vector<vector<bool>> leftToVisit;  // for exploration
// distOptional will be used for miscellaneous purposes
vector<vector<int>> distFromMyBase, distFromOppBase, distOptional;
bool reachedFirstCell[5];
pair<int, int> firstAssignment[5];
// for checking if two are in same line, only
// checking x same or y same is not enough
// there can be walls
vector<vector<int>> prefixSumRow, prefixSumCol;
int backandforthcounter[5];
//  vector<vector<pair<int, int>>> parent;
enum opTypes { MOVE, FIRE, FREEZE, MINE, WAIT };

int mandatoryCarrier = -1, flagDefender = -1;

int dx[] = {1, 0, -1, 0};
int dy[] = {0, 1, 0, -1};

inline void ign() { cin.ignore(); }

struct Powerup {
    string name;
    int price, damage;
    inline void readPowerUp() {
        cin >> name >> price >> damage;
        ign();
    }
};

struct Path {
    int dist, curve;
    pair<int, int> parent, grand_parent;
    // grandparent is the second part for tracking curve
    void initiate() {
        dist = INF;
        curve = 0;
        grand_parent = parent = {-1, -1};
    }
    bool operator<(const Path& other) {
        if (dist != other.dist) return dist < other.dist;
        else if (curve != other.curve) return curve < other.curve;
        else return false;
    }
};

struct CellInAPath {
    vector<Path> top;
    int pop_count;
    // each cell will store the top 5 shortest paths to it
    //{dist, curve} pair
    void initiate() {
        top.resize(5);
        for (int i = 0; i < top.size(); i++) {
            top[i].initiate();
        }
        pop_count = 0;
    }
    void callSorting() { sort(top.begin(), top.end()); }
};

vector<vector<CellInAPath>> myShortestPath;

Powerup powerups[3];  // 0 fire, 1 freeze, 2 mine

struct Coin {
    int posX, posY;
    bool taken;
    inline void readCoin() {
        cin >> posX >> posY;
        ign();
        taken = false;
    }
};

vector<Coin> assignedCoins;  // explorer minion দের closest coin টা assign করে দেয়া হবে, যদি থাকে

struct Minion {
    // int id; this is basically the array index
    int posX, posY, health, timeout;
    // read the id first, and then call minion[id].readMinion()
    inline void readMinion() {
        cin >> posX >> posY >> health >> timeout;
        ign();
        pair<int, int> p = {posX, posY};
        leftToVisit[posX][posY] = false;
        for (int i = 0; i < n_minions; i++) {
            if (posX == assignedCoins[i].posX &&
                posY == assignedCoins[i].posY) {  //নেয়া হয়ে গেসে coin টা
                assignedCoins[i].posX = -1;
                assignedCoins[i].posY = -1;
            }
        }
    }
};

struct Operation {
    opTypes ty;
    int id, x, y;
    Operation(opTypes o, int id, int x = -1, int y = -1) : ty(o), id(id), x(x), y(y) {}
    void print() {
        if (ty == MOVE) cout << "MOVE " << id << " " << x << " " << y;
        else if (ty == MINE) cout << "MINE " << id << " " << x << " " << y;
        else if (ty == FIRE) cout << "FIRE " << id;
        else if (ty == FREEZE) cout << "FREEZE " << id;
        else cout << "WAIT " << id;
    }
};

void readGrid() {
    cin >> height >> width;
    ign();
    grid.resize(height);
    for (int i = 0; i < height; i++) {
        string s;
        cin >> s;
        grid[i] = s;
        ign();
    }
    // for O(1) checking if any wall in between
    prefixSumRow.assign(height, vector<int>(width, 0));
    prefixSumCol.assign(height, vector<int>(width, 0));
    // wall 1, normal cell 0
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            if (col == 0) prefixSumRow[row][col] = (grid[row][col] == '#');
            else prefixSumRow[row][col] = prefixSumRow[row][col - 1] + (grid[row][col] == '#');
        }
    }
    for (int col = 0; col < width; col++) {
        for (int row = 0; row < height; row++) {
            if (row == 0) prefixSumCol[row][col] = (grid[row][col] == '#');
            else prefixSumCol[row][col] = prefixSumCol[row - 1][col] + (grid[row][col] == '#');
        }
    }
    // cerr << "Pref Row:" << endl;
    // for (int i = 0; i < height; i++) {
    //     for (int j = 0; j < width; j++) {
    //         cerr << prefixSumRow[i][j] << " ";
    //     }
    //     cerr << endl;
    // }
    // cerr << "Pref Col:" << endl;
    // for (int i = 0; i < height; i++) {
    //     for (int j = 0; j < width; j++) {
    //         cerr << prefixSumCol[i][j] << " ";
    //     }
    //     cerr << endl;
    // }
}

void readBases() {
    cin >> myFlagBaseX >> myFlagBaseY;
    ign();
    cin >> oppFlagBaseX >> oppFlagBaseY;
    ign();
}

void readPowerUps() {
    for (int i = 0; i < 3; i++) powerups[i].readPowerUp();
}

inline bool isValid(int x, int y) {
    return x >= 0 && x < height && y >= 0 && y < width && grid[x][y] == '.';
}

inline bool sameLine(int x1, int y1, int x2, int y2, bool wantAdjAnalysis, int lastX, int lastY) {
    // cerr << "x1: " << x1 << " y1: " << y1 << " x2: " << x2 << " y2: " << y2
    //      << " " << lastX << " " << lastY << endl;
    //  x2 y2 is the target
    bool ret = false;
    int now = -1;
    if (x1 == x2 && y1 == y2) return true;
    // first ensure সে এখানে বসে নাই ইচ্ছা করে, কারণ তা হলে যাওয়ার কয়টা জায়গা আছে
    // matter করে না, বসেই থাকবে যেহেতু
    if (wantAdjAnalysis && (lastX == -1 || (x2 != lastX || y2 != lastY))) {
        // lastX = -1 হলে last এর data নাই
        // so do the direct comparison
        // so dealing with an opponent
        //আমি তাকে যে লাইন ধরে attack করবো, তার যদি ওই লাইন ছেড়ে যাওয়ার ability
        //থাকে, তাহলে ওই attack করে লাভ নাই
        if (x1 == x2)
            if (isValid(x2 + 1, y2) || isValid(x2 - 1, y2)) return ret;
        if (y1 == y2)
            if (isValid(x2, y2 + 1) || isValid(x2, y2 - 1)) return ret;
    }
    if (x1 == x2) {  // horizontal matching
        //উল্টা হলে negative আসবে, সমস্যা কী?
        now = prefixSumRow[x2][y2];
        if (y1 != 0) now -= prefixSumRow[x1][y1 - 1];
        if (now == 0) ret = true;
    } else if (y1 == y2) {  // vertical matching
        now = prefixSumCol[x2][y2];
        if (x1 != 0) now -= prefixSumCol[x1 - 1][y1];
        if (now == 0) ret = true;
    }
    // cerr << now << " " << (ret ? "true" : "false") << endl;
    return ret;
}

void bfs(pair<int, int> s, vector<vector<int>>& dist) {
    visited.assign(height, vector<bool>(width, false));
    dist.assign(height, vector<int>(width, INF));
    // parent.assign(height, vector<pair<int, int>>(width, {-1, -1}));
    queue<pair<int, int>> q;

    dist[s.first][s.second] = 0;
    q.push(s);

    while (!q.empty()) {
        pair<int, int> now = q.front();
        q.pop();
        if (visited[now.first][now.second]) continue;
        visited[now.first][now.second] = true;
        vector<int> v{0, 1, 2, 3};
        random_shuffle(v.begin(), v.end());
        for (int dir = 0; dir < 4; dir++) {
            int newX = dx[v[dir]] + now.first;
            int newY = dy[v[dir]] + now.second;

            if (!isValid(newX, newY) || visited[newX][newY]) continue;
            dist[newX][newY] = dist[now.first][now.second] + 1;
            // parent[newX][newY] = {now.first, now.second};
            q.push({newX, newY});
        }
    }
}

void special_bfs(pair<int, int> s) {
    myShortestPath.resize(height, vector<CellInAPath>(width));
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            myShortestPath[i][j].initiate();
        }
    }
    queue<pair<int, int>> q;
    myShortestPath[s.first][s.second].top[0].dist = 0;
    q.push(s);

    while (!q.empty()) {
        auto now = q.front();
        q.pop();
        myShortestPath[now.first][now.second].pop_count++;
        if (myShortestPath[now.first][now.second].pop_count > 5) continue;
        vector<int> v{0, 1, 2, 3};
        random_shuffle(v.begin(), v.end());
        for (int dir = 0; dir < 4; dir++) {
            int newX = now.first + dx[v[dir]];
            int newY = now.second + dy[v[dir]];
            if (!isValid(newX, newY)) continue;
            bool upd = false;
            int start_idx = 0;
            for (int j = 0; j < 5; j++) {
                if (myShortestPath[newX][newY].top[j].dist >=
                    myShortestPath[now.first][now.second].top[0].dist) {
                    break;
                } else start_idx++;
            }
            if (start_idx == 5) continue;
            for (int j = 0; j < 5; j++) {
                if (start_idx + j >= 5) break;
                auto cur = myShortestPath[now.first][now.second].top[j];
                auto to_update = myShortestPath[newX][newY].top[j + start_idx];
                auto new_val = cur;
                new_val.dist++;
                new_val.parent = {now.first, now.second};
                new_val.grand_parent = cur.parent;
                if (new_val.grand_parent.first != -1 && newX != new_val.grand_parent.first &&
                    newY != new_val.grand_parent.second) {
                    new_val.curve++;
                }
                if (new_val < to_update) {
                    upd = true;
                    myShortestPath[newX][newY].top[j + start_idx] = new_val;
                    q.push({newX, newY});
                }
            }
            if (upd) myShortestPath[newX][newY].callSorting();
        }
    }
}

void dfs(int x, int y) {
    visited[x][y] = true;
    not_visited.push_back({x, y});
    vector<int> v{0, 1, 2, 3};
    random_shuffle(v.begin(), v.end());
    int ddx[] = {-1, 0, 0, 1};
    int ddy[] = {0, 1, -1, 0};
    if (myFlagBaseY * 2 <= width) {
        swap(ddx[0], ddx[3]);
        swap(ddy[0], ddy[3]);
    }
    for (int dir = 0; dir < 4; dir++) {
        int newX = ddx[dir] + x;
        int newY = ddy[dir] + y;
        if (!isValid(newX, newY) || visited[newX][newY]) continue;
        dfs(newX, newY);
    }
}

void init() {
    // explorer minions will just explore for more and more coins
    int startX, startY;
    //না, start কোনো এক কোণা থেকে শুরু করলে ভালো মনেহয়
    for (int i = 0; i < height; i++) {
        for (int j = width - 1; j >= 0; j--) {
            if (!visited[i][j]) continue;
            startX = i;
            startY = j;
            break;
        }
    }
    visited.assign(height, vector<bool>(width, false));
    cerr << "DFS Start: " << startX << " " << startY << endl;
    dfs(startX, startY);
    leftToVisit.assign(height, vector<bool>(width, true));  //শুরুতে সবাই left to visit
    // cerr << "dfs done" << endl;

    //নিজেদের shortest path বানাই চল, যেখানে সবচেয়ে বেশি curved path কে prefer করবো
    special_bfs({myFlagBaseX, myFlagBaseY});
}

struct Game {
    int myScore, oppScore;
    int myFlagX, myFlagY, oppFlagX, oppFlagY;
    int myFlagCarrier, oppFlagCarrier;

    int myAliveMinionCnt;
    vector<Minion> myMinions;
    bitset<5> isAlive;

    int visibleMinionCnt;
    vector<Minion> oppMinions;
    bitset<5> isOppVisible;

    int visibleCoinCnt;
    vector<Coin> visibleCoins;

    vector<Operation> operations;

    Game() {
        myMinions.resize(5);
        oppMinions.resize(5);
    }

    // initial things
    inline void readFlag(bool my) {
        if (my) {
            cin >> myFlagX >> myFlagY;
            cin >> myFlagCarrier;
        } else {
            cin >> oppFlagX >> oppFlagY;
            cin >> oppFlagCarrier;
            if (oppFlagCarrier != -1 && oppFlagCarrier != mandatoryCarrier) {
                int prev = mandatoryCarrier;
                if (oppFlagCarrier == flagDefender) flagDefender = mandatoryCarrier;
                mandatoryCarrier = oppFlagCarrier;
                //না চাইতেই নিয়ে যখন নিসে, ওকেই দায়িত্ব দাও
                if (explorerMinions.count(mandatoryCarrier)) {
                    explorerMinions.erase(mandatoryCarrier);
                    explorerMinions.insert(prev);
                }
            }
        }
        ign();
    }

    // game loop things
    inline void readScores() {
        cin >> myScore >> oppScore;
        ign();
    }

    void readMyMinions(const Game& last, const Game& last2) {
        isAlive.reset();
        cin >> myAliveMinionCnt;
        ign();

        for (int i = 0; i < myAliveMinionCnt; i++) {
            int id;
            cin >> id;
            ign();
            isAlive[id] = true;
            myMinions[id].readMinion();
            int xx = myMinions[id].posX;
            int yy = myMinions[id].posY;
            if (xx == firstAssignment[id].first && yy == firstAssignment[id].second)
                reachedFirstCell[id] = true;
            if (!firstTime && xx == last2.myMinions[id].posX && yy == last2.myMinions[id].posY) {
                if (backandforthcounter[id] == 0) backandforthcounter[id] = 2;
                else backandforthcounter[id]++;
            } else backandforthcounter[id] = 0;
        }

        if ((last.myAliveMinionCnt - 1 == myAliveMinionCnt) && myAliveMinionCnt <= 2) {
            explorerMinions.insert(flagDefender);
            int sz = 0, idx = -1;
            for (int j = 0; j < n_minions - 2; j++) {
                if (idx == -1 || cellsToExplore[j].size() > sz) {
                    sz = cellsToExplore[j].size();
                    idx = j;
                }
            }
            assignedExploration[flagDefender] = idx;
            cerr << "Assigned " << idx << " to " << flagDefender << endl;
            flagDefender = -1;
        }

        if (n_minions == -1) {
            n_minions = myAliveMinionCnt;
            for (int i = 0; i < n_minions; i++) explorerMinions.insert(i);
            // need a flag defender
            // choose the minion that is closest to my flag (my base)
            flagDefender = 0;
            int highest_distance = distFromMyBase[myMinions[0].posX][myMinions[0].posY];
            // but 0 might be blocked somewhere
            while (distFromMyBase[myMinions[flagDefender].posX][myMinions[flagDefender].posY] >=
                   1000)
                flagDefender++;
            for (int i = 0; i < n_minions; i++) {
                int xx = myMinions[i].posX;
                int yy = myMinions[i].posY;
                if (distFromMyBase[xx][yy] > highest_distance) {
                    highest_distance = distFromMyBase[xx][yy];
                    flagDefender = i;
                }
            }
            explorerMinions.erase(flagDefender);

            // explorer দের কাজ distribute করে দেয়া লাগবে properly
            cellsToExplore.resize(n_minions);
            int per = height / (n_minions - 2);
            assignedExploration.assign(n_minions, -1);
            for (int now = 0; now < not_visited.size(); now++) {
                // cellsToExplore[idx].push_back(not_visited[now]);
                int nowx = not_visited[now].first;
                int nowy = not_visited[now].second;
                if (nowx == myFlagBaseX && nowy == myFlagBaseY) continue;
                if (nowx == oppFlagBaseX && nowy == oppFlagBaseY) continue;
                int div = nowx / per;
                if (div >= n_minions - 3) cellsToExplore[n_minions - 3].push_back(not_visited[now]);
                else if (div == 0) cellsToExplore[0].push_back(not_visited[now]);
                else if (div == 1) cellsToExplore[1].push_back(not_visited[now]);
                else cellsToExplore[2].push_back(not_visited[now]);
            }

            // coin assignent initialization
            assignedCoins.resize(n_minions);
            for (int i = 0; i < assignedCoins.size(); i++) {
                assignedCoins[i].posX = -1;
                assignedCoins[i].posY = -1;
            }
        }

        if (mandatoryCarrier == -1 || !isAlive[mandatoryCarrier]) {
            // choose the minion that is the closest to opponent flag
            distOptional = distFromOppBase;
            if (oppFlagBaseX != oppFlagX || oppFlagBaseY != oppFlagY)
                bfs({oppFlagX, oppFlagY}, distOptional);
            int least_distance = -1;
            for (int i = 0; i < n_minions; i++) {
                if (!isAlive[i]) continue;
                int xx = myMinions[i].posX;
                int yy = myMinions[i].posY;
                if (distOptional[xx][yy] >= 1000) continue;
                // if (myAliveMinionCnt >= 2 && i == flagDefender) continue;
                if (least_distance == -1 || distOptional[xx][yy] < least_distance) {
                    mandatoryCarrier = i;
                    least_distance = distOptional[xx][yy];
                }
            }
            if (flagDefender == mandatoryCarrier) {
                // need to set a new flag defender
                flagDefender = -1;
                if (myAliveMinionCnt > 2 || (myAliveMinionCnt == 2 && n_minions > 3)) {
                    distOptional = distFromMyBase;
                    if (myFlagBaseX != myFlagX || myFlagBaseY != myFlagY)
                        bfs({myFlagX, myFlagY}, distOptional);
                    int least_distance = -1;
                    for (int i = 0; i < n_minions; i++) {
                        if (!isAlive[i] || blocked.count(i)) continue;
                        int xx = myMinions[i].posX;
                        int yy = myMinions[i].posY;
                        if (i == mandatoryCarrier) continue;
                        if (distOptional[xx][yy] >= 1000) continue;
                        if (least_distance == -1 || distOptional[xx][yy] < least_distance) {
                            flagDefender = i;
                            least_distance = distOptional[xx][yy];
                        }
                    }
                }
            }
            if (explorerMinions.count(mandatoryCarrier)) explorerMinions.erase(mandatoryCarrier);
        }

        if ((flagDefender != -1 && (!isAlive[flagDefender] && myAliveMinionCnt >= 2))) {
            // if there are >= 2 minions, then one can be flag defender,
            // right? choose the minion that is the closest to my flag
            distOptional = distFromMyBase;
            if (myFlagBaseX != myFlagX || myFlagBaseY != myFlagY)
                bfs({myFlagX, myFlagY}, distOptional);
            int least_distance = -1;
            for (int i = 0; i < n_minions; i++) {
                if (!isAlive[i]) continue;
                int xx = myMinions[i].posX;
                int yy = myMinions[i].posY;
                if (i == mandatoryCarrier) continue;
                if (distOptional[xx][yy] >= 1000) continue;
                if (least_distance == -1 || distOptional[xx][yy] < least_distance) {
                    flagDefender = i;
                    least_distance = distOptional[xx][yy];
                }
            }
            // explorerMinions.erase(mandatoryCarrier);
        }
        if (explorerMinions.count(mandatoryCarrier)) explorerMinions.erase(mandatoryCarrier);
        if (explorerMinions.count(flagDefender)) explorerMinions.erase(flagDefender);

        if (assignedExploration.back() == -1) {
            //কে কোনটা explore করবে এটা শুরুতে ঠিক করে দাও
            assignedExploration.back() = 0;
            int id = 0;
            assignedExploration[mandatoryCarrier] = n_minions - 2;
            assignedExploration[flagDefender] = 0;
            for (int j = 0; j < n_minions; j++) {
                if (distFromMyBase[myMinions[j].posX][myMinions[j].posY] >= 1000) {
                    blocked.insert(j);
                }
            }
            int per = height / (n_minions - 2);
            for (int i = 0; i < n_minions; i++) {
                if (i == mandatoryCarrier || i == flagDefender) {
                    reachedFirstCell[i] = true;  //এদের দুইজনের এতো spread করা লাগবে না
                    firstAssignment[i] = {-1, -1};
                    continue;
                }
                // cerr << "assigned " << id << " to " << i << endl;
                assignedExploration[i] = id;
                bfs({myMinions[i].posX, myMinions[i].posY}, distOptional);
                firstAssignment[i] = {-1, -1};
                for (auto they : cellsToExplore[id]) {
                    if (firstAssignment[i].first == -1) firstAssignment[i] = they;
                    else if (distOptional[they.first][they.second] <
                                 distOptional[firstAssignment[i].first]
                                             [firstAssignment[i].second] &&
                             (they.second * 3 >= width) && (they.second * 3 <= 2 * width)) {
                        firstAssignment[i] = they;
                    }
                }
                if (firstAssignment[i] == make_pair(myMinions[i].posX, myMinions[i].posY))
                    reachedFirstCell[i] = true;
                cerr << "Minion ID: " << i << " Assigned Set: " << id << endl;
                cerr << "First Assignment: " << firstAssignment[i].first << " "
                     << firstAssignment[i].second << " ";
                cerr << "Allotted Cells: " << cellsToExplore[id].size() << endl;
                id++;
            }
        }
    }

    void readOppMinions() {
        isOppVisible.reset();
        cin >> visibleMinionCnt;
        ign();
        for (int i = 0; i < visibleMinionCnt; i++) {
            int id;
            cin >> id;
            ign();
            isOppVisible[id] = true;
            oppMinions[id].readMinion();
        }
    }

    void readCoins() {
        cin >> visibleCoinCnt;
        ign();
        visibleCoins.resize(visibleCoinCnt);
        for (int i = 0; i < visibleCoinCnt; i++) {
            visibleCoins[i].readCoin();
            for (int j = 0; j < n_minions; j++) {
                if (assignedCoins[j].posX == visibleCoins[i].posX &&
                    assignedCoins[j].posY == visibleCoins[i].posY)
                    visibleCoins[i].taken = true;
            }
        }
        // cerr << "Read Coins done" << endl;
    }

    void takeOperation(opTypes t, int id, bool& marker, int x = -1, int y = -1) {
        if (moveDone) return;
        int cost = 0;
        if (t == FIRE) cost = powerups[0].price;
        else if (t == FREEZE) cost = powerups[1].price;
        else if (t == MINE) cost = powerups[2].price;
        if (myScore < cost) return;
        operations.push_back(Operation(t, id, x, y));
        marker = true;
    }

    bool askSameLine(int x1, int y1, int x2, int y2, bool wantAdjAnalysis, int targetID,
                     const Game& last) {
        // cerr << "x1: " << x1 << " y1: " << y1 << " x2: " << x2 << " y2: " << y2 << " "
        //      << (wantAdjAnalysis ? "true" : "false") << " " << last.isOppVisible[targetID] <<
        //      endl;
        int lastX = -1, lastY = -1;
        if (wantAdjAnalysis && !firstTime && last.isOppVisible[targetID]) {
            lastX = last.oppMinions[targetID].posX;
            lastY = last.oppMinions[targetID].posY;
        }
        if (wantAdjAnalysis) return sameLine(x1, y1, x2, y2, true, lastX, lastY);
        else return sameLine(x1, y1, x2, y2, false, x2, y2);
    }

    pair<int, int> giveBestCell(int x, int y) {
        // so around one cell, there are multiple adjacent cells.
        // sometimes it is better to stand in one of those adjacent cells other than the cell
        // itself, so that there is a better view of things
        pair<int, int> d[] = {{0, 0},  {1, 1},   {1, 0},  {1, -1}, {0, 1},
                              {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}};
        // pair<int, int> d[] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {0, 0}};
        pair<int, int> curBest = {-1, -1};
        int bestSee = 0;
        for (int dir = 0; dir < 9; dir++) {
            int newX = x + d[dir].first;
            int newY = y + d[dir].second;
            if (!isValid(newX, newY)) continue;
            int nowSee = 1;  // the cell itself
            // vertical bottom
            int low = newX + 1, high = height - 1;
            int ans = 0;  // how many cells
            while (low <= high) {
                int mid = (low + high) >> 1;
                if (prefixSumCol[mid][newY] - prefixSumCol[newX][newY] == 0) {
                    ans = mid - newX;  // excluding current cell
                    low = mid + 1;
                } else high = mid - 1;
            }
            nowSee += ans;
            // cerr << "Vertical Bottom: " << ans << endl;
            // vertical up
            low = 0, high = newX - 1;
            ans = 0;  // how many cells
            while (low <= high) {
                int mid = (low + high) >> 1;
                if (prefixSumCol[newX - 1][newY] - (mid >= 1 ? prefixSumCol[mid - 1][newY] : 0) ==
                    0) {
                    ans = newX - mid;  // excluding current cell
                    high = mid - 1;
                } else low = mid + 1;
            }
            // cerr << "Vertical Up: " << ans << endl;
            nowSee += ans;
            // horizontal right
            low = newY + 1, high = width - 1;
            ans = 0;  // how many cells
            while (low <= high) {
                int mid = (low + high) >> 1;
                if (prefixSumRow[newX][mid] - prefixSumRow[newX][newY] == 0) {
                    ans = mid - newY;  // excluding current cell
                    low = mid + 1;
                } else high = mid - 1;
            }
            // cerr << "Horizontal Right: " << ans << endl;
            nowSee += ans;
            // horizontal left
            low = 0, high = newY - 1;
            ans = 0;  // how many cells
            while (low <= high) {
                int mid = (low + high) >> 1;
                if (prefixSumRow[newX][newY - 1] - (mid >= 1 ? prefixSumRow[newX][mid - 1] : 0) ==
                    0) {
                    ans = newY - mid;  // excluding current cell
                    high = mid - 1;
                } else low = mid + 1;
            }
            // cerr << "Horizontal Left: " << ans << endl;
            nowSee += ans;
            // cerr << "Cell: " << newX << " " << newY << " can see " << nowSee << " cells" <<
            // endl;
            if (nowSee > bestSee) {
                bestSee = nowSee;
                curBest = {newX, newY};
            }
        }
        cerr << x << " " << y << " Adj Best: " << curBest.first << " " << curBest.second << endl;
        return curBest;
    }

    void calculateVisibleMinions(int id, pair<int, int>& affected, pair<int, int>& death,
                                 pair<int, int>& alreadyFrozen, const Game& last,
                                 bool wantAnalysis) {
        // how many minions from both sides this minion of mine affects...
        affected = death = alreadyFrozen = {0, 0};
        int xx = myMinions[id].posX;
        int yy = myMinions[id].posY;
        // cerr << "ID: " << id << " position: " << xx << " " << yy << endl;
        for (int i = 0; i < n_minions; i++) {
            if (i == id || !isAlive[i]) continue;
            bool ret = askSameLine(xx, yy, myMinions[i].posX, myMinions[i].posY, false, i, last);
            if (ret) {
                // cerr << "My: " << id << " and " << i << " in same line" << endl;
                affected.first++;
                if (myMinions[i].health <= powerups[0].damage) death.first++;
                if (myMinions[i].timeout > 0) alreadyFrozen.first++;
            }
        }
        for (int i = 0; i < n_minions; i++) {
            if (!isOppVisible[i]) continue;
            bool ret =
                askSameLine(xx, yy, oppMinions[i].posX, oppMinions[i].posY, wantAnalysis, i, last);
            if (ret) {
                // cerr << "Opp: " << id << " and " << i << " in same line" << endl;
                affected.second++;
                if (oppMinions[i].health <= powerups[0].damage) death.second++;
                if (oppMinions[i].timeout > 0) alreadyFrozen.second++;
            }
        }
    }

    vector<int> getVisibleMinions(int id, const Game& last) {
        vector<int> v;
        int xx = myMinions[id].posX;
        int yy = myMinions[id].posY;
        for (int i = 0; i < n_minions; i++) {
            if (!isOppVisible[i]) continue;
            if (askSameLine(xx, yy, oppMinions[i].posX, oppMinions[i].posY, false, -1, last))
                v.push_back(i);
        }
        sort(v.begin(), v.end(), [&](const int a, const int b) {
            int d1 = abs(xx - oppMinions[a].posX) + abs(yy - oppMinions[a].posY);
            int d2 = abs(xx - oppMinions[b].posX) + abs(yy - oppMinions[b].posY);
            return d1 < d2;
        });
        return v;
    }

    void printOperations() {
        const int nn = operations.size();
        // cerr << "operations getting printed here" << endl;
        for (int i = 0; i < nn; i++) {
            operations[i].print();
            if (i == nn - 1) cout << endl;
            else cout << " | ";
        }
    }

    void doThings(const Game& last, const Game& last2) {
        vector<int> v;
        // fixing the priorities
        if (mandatoryCarrier != -1) v.push_back(mandatoryCarrier);
        if (flagDefender != mandatoryCarrier && flagDefender != -1) v.push_back(flagDefender);

        for (int i = 0; i < n_minions; i++) {
            auto it = find(v.begin(), v.end(), i);
            if (it == v.end()) v.push_back(i);
        }

        for (int i = 0; i < v.size(); i++) cerr << v[i] << " ";
        cerr << endl;

        for (int j = 0; j < n_minions; j++) {
            int i = v[j];
            if (!isAlive[i]) continue;  // dead already
            if (blocked.count(i)) {
                takeOperation(WAIT, i, moveDone);
            }

            int xx = myMinions[i].posX;
            int yy = myMinions[i].posY;

            pair<int, int> a1, d1, f1, a2, d2, f2;
            calculateVisibleMinions(i, a1, d1, f1, last, true);   // with adj analysis
            calculateVisibleMinions(i, a2, d2, f2, last, false);  // without adj analysis

            auto visibleOnes = getVisibleMinions(i, last);
            int maxOppHealth = 0, needed_fire_spell = 0;
            if (!visibleOnes.empty()) {
                maxOppHealth = oppMinions[visibleOnes.front()].health;
                for (int k = 1; k < visibleOnes.size(); k++)
                    maxOppHealth = max(maxOppHealth, oppMinions[visibleOnes[k]].health);
                needed_fire_spell = (maxOppHealth + powerups[0].damage - 1) / powerups[0].damage;
            }

            cerr << "ID: " << i << ", ";
            if (i == mandatoryCarrier) cerr << "mandatory carrier ";
            else if (i == flagDefender) cerr << "flag defender ";
            else cerr << "explorer ";
            cerr << xx << " " << yy << endl;

            // cerr << "Assigned Coin: " << assignedCoins[i].posX << " " <<
            // assignedCoins[i].posY
            //      << endl;

            // cerr << distFromMyBase[xx][yy] << " " <<
            // distFromOppBase[xx][yy]
            //      << endl;

            // cerr << "Visible opponent minions: ";
            // for (int i = 0; i < visibleOnes.size(); i++) {
            //     cerr << visibleOnes[i] << " ";
            // }
            // cerr << endl;

            cerr << "1: {" << a1.first << ", " << a1.second << "}, {" << d1.first << ", "
                 << d1.second << "}, {" << f1.first << ", " << f1.second << "}"
                 << " 2: ";
            cerr << "{" << a2.first << ", " << a2.second << "}, {" << d2.first << ", " << d2.second
                 << "}, {" << f2.first << ", " << f2.second << "}" << endl;

            // cerr << "Curve and distances from this position: " << endl;
            // for (int j = 0; j < 5; j++) {
            //     cerr << "{" << myShortestPath[xx][yy].top[j].dist << ", "
            //          << myShortestPath[xx][yy].top[j].curve << "}";
            //     if (j != 4) cerr << ", ";
            //     else cerr << endl;
            // }

            // cerr << maxOppHealth << " " << needed_fire_spell << endl;
            moveDone = false;
            auto& ref = myShortestPath[xx][yy].top;

            int movX = -1, movY = -1;
            if (i == mandatoryCarrier) {
                if (oppFlagCarrier == -1) movX = oppFlagX, movY = oppFlagY;
                else movX = ref[0].parent.first, movY = ref[0].parent.second;
            } else if (i == flagDefender) {
                if (myFlagCarrier != -1) movX = myFlagX, movY = myFlagY;
                else {
                    pair<int, int> p = giveBestCell(myFlagX, myFlagY);
                    movX = p.first, movY = p.second;
                }
            }
            // cerr << "Prev: " << movX << " " << movY << " After: ";

            bool nullify = false;
            if (!firstTime && i != mandatoryCarrier) {
                for (int j = 0; j < n_minions; j++) {
                    if (!last2.isOppVisible[j] || !isOppVisible[j]) continue;
                    if (!askSameLine(xx, yy, oppMinions[j].posX, oppMinions[j].posY, false, -1,
                                     last))
                        continue;
                    // let me check if this opponent minion is in attacking mood
                    if (askSameLine(last2.myMinions[i].posX, last2.myMinions[i].posY,
                                    last2.oppMinions[j].posX, last2.oppMinions[j].posY, false, -1,
                                    last)) {
                        if (myMinions[i].health == last2.myMinions[i].health) nullify = true;
                    }
                }
            }

            bool found = false;
            for (int j = 0; j < n_minions; j++) {
                if (!isOppVisible[j]) continue;
                if (oppMinions[j].timeout == 1 &&
                    askSameLine(xx, yy, oppMinions[i].posX, oppMinions[i].posY, false, -1, last)) {
                    found = true;
                    break;
                }
            }
            if (!found && f2.second == a2.second) nullify = true;
            if (i == mandatoryCarrier && backandforthcounter[i] >= 2 &&
                oppScore >= last2.oppScore - 1)
                nullify = true;
            if (i == mandatoryCarrier && oppFlagCarrier == -1 && myFlagCarrier != -1 &&
                distFromMyBase[myFlagX][myFlagY] >= 4)
                nullify = true;
            if (i == mandatoryCarrier && myAliveMinionCnt <= 2) nullify = true;
            if (!nullify) {
                int distNow = distFromMyBase[xx][yy];
                int score = INF;
                int dxx[] = {1, 0, -1, 0};
                int dyy[] = {0, 1, 0, -1};
                for (int dir = 0; dir < 4; dir++) {
                    int newX = dxx[dir] + xx;
                    int newY = dyy[dir] + yy;
                    if (!isValid(newX, newY)) continue;
                    bool ok = true;
                    // first find out who is guarding the flag, if any
                    for (int j = 0; j < n_minions && ok; j++) {
                        if (!isOppVisible[j]) continue;
                        if (!askSameLine(xx, yy, oppMinions[j].posX, oppMinions[j].posY, false, -1,
                                         last))
                            continue;
                        // if (i == mandatoryCarrier && oppFlagCarrier == -1) {
                        //     if (!askSameLine(oppMinions[j].posX, oppMinions[j].posY,
                        //     oppFlagX, oppFlagY, false, -1, last)) continue;
                        // }
                        if (askSameLine(newX, newY, oppMinions[j].posX, oppMinions[j].posY, false,
                                        -1, last))
                            ok = false;
                    }
                    if (!ok) continue;
                    ;
                    if (distFromMyBase[newX][newY] < score) {
                        score = distFromMyBase[newX][newY];
                        movX = newX;
                        movY = newY;
                    }
                }
            }
            cerr << movX << " " << movY << endl;

            // moveDone check will be done inside the function
            if (i == mandatoryCarrier) {
                if (i == oppFlagCarrier) {
                    // carrying the flag
                    if (oppScore >= min(powerups[0].price, powerups[1].price) && a2.second > 0) {
                        // opponent can attack
                        // take no risk as you are carrying the flag
                        // first check if there are any opponent minions whose timeout is going
                        // to end now
                        if (found || (f2.second < a2.second)) {
                            // someone will get unfrozen in the next moment or is already
                            // unfrozen
                            if (myScore >= powerups[1].price) takeOperation(FREEZE, i, moveDone);
                            else if (backandforthcounter[i] <= 4) {
                                takeOperation(MOVE, i, moveDone, movX, movY);
                            } else {
                                takeOperation(MOVE, i, moveDone, ref[0].parent.first,
                                              ref[0].parent.second);
                            }
                        } else {
                            takeOperation(MOVE, i, moveDone, ref[0].parent.first,
                                          ref[0].parent.second);
                        }
                    }

                    else {
                        if (a2.second - f2.second > 0 && myScore >= powerups[1].price)
                            takeOperation(FREEZE, i, moveDone);
                        // opponent does not have the ability to attack
                        else
                            takeOperation(MOVE, i, moveDone, ref[0].parent.first,
                                          ref[0].parent.second);
                        //এই freeze টা জমায় রেখে পরে করলেও তো চলে
                    }
                } else {
                    // not got the flag yet
                    bool encounter = false;
                    for (int j = 0; j < n_minions; j++) {
                        if (!isOppVisible[j]) continue;
                        if (!askSameLine(oppMinions[j].posX, oppMinions[j].posY, oppFlagX, oppFlagY,
                                         false, -1, last))
                            continue;
                        if (askSameLine(xx, yy, oppMinions[j].posX, oppMinions[j].posY, false, -1,
                                        last))
                            encounter = true;
                    }
                    int max_forth;
                    if (last2.oppScore > oppScore && abs(last2.oppScore - oppScore) >= 4 &&
                        oppScore >= powerups[0].price) {
                        max_forth =
                            2 * ((oppScore + powerups[0].price - 1) / powerups[0].price) + 1;
                    } else if (abs(last2.oppScore - oppScore) < 4) {
                        max_forth = 4;
                    }
                    cerr << max_forth << endl;
                    if (!encounter) {
                        // not encountering some flag defender
                        if (a2.second > 0 && oppScore >= powerups[0].price) {
                            // may attack me
                            if (backandforthcounter[i] <= max_forth) {
                                if (myScore < powerups[1].price) {  //টাকা নাই
                                    takeOperation(MOVE, i, moveDone, movX, movY);
                                } else if (movX != oppFlagX || movY != oppFlagY) {
                                    takeOperation(MOVE, i, moveDone, movX, movY);
                                } else if (last.myMinions[i].health > myMinions[i].health ||
                                           last.myMinions[i].timeout > 0) {
                                    if (myScore >= powerups[1].price && f2.second < a2.second)
                                        takeOperation(FREEZE, i, moveDone);
                                    else if (myAliveMinionCnt <= 2 && myScore >= powerups[1].price)
                                        takeOperation(FREEZE, i, moveDone);
                                }
                                takeOperation(MOVE, i, moveDone, oppFlagX, oppFlagY);
                            } else {
                                //অনেক হইসে, এবার সামনে আগাও -_-
                                cerr << "Whoa" << endl;
                                takeOperation(MOVE, i, moveDone, oppFlagX, oppFlagY);
                            }
                        } else {
                            takeOperation(MOVE, i, moveDone, oppFlagX, oppFlagY);
                        }

                    } else {
                        // encountering a flag defender
                        bool found = false;
                        for (int j = 0; j < n_minions; j++) {
                            if (!isOppVisible[j]) continue;
                            if (oppMinions[j].timeout == 1 &&
                                askSameLine(xx, yy, oppMinions[i].posX, oppMinions[i].posY, false,
                                            -1, last)) {
                                found = true;
                                break;
                            }
                        }

                        if (a2.second > 0 && oppScore >= powerups[0].price) {
                            // may attack me
                            if (backandforthcounter[i] <= max_forth) {
                                if (myScore < powerups[1].price) {  //টাকা নাই
                                    takeOperation(MOVE, i, moveDone, movX, movY);
                                } else if (movX != oppFlagX || movY != oppFlagY) {
                                    takeOperation(MOVE, i, moveDone, movX, movY);
                                } else if (last.myMinions[i].health > myMinions[i].health ||
                                           last.myMinions[i].timeout > 0) {
                                    if (myScore >= powerups[1].price && f2.second < a2.second)
                                        takeOperation(FREEZE, i, moveDone);
                                }
                                takeOperation(MOVE, i, moveDone, movX, movY);
                            } else {
                                //অনেক হইসে, এবার সামনে আগাও -_-
                                if (a2.second - f2.second > 0 || found) {
                                    if (myScore >= powerups[1].price)
                                        takeOperation(FREEZE, i, moveDone);
                                }
                                takeOperation(MOVE, i, moveDone, oppFlagX, oppFlagY);
                            }
                        } else {
                            takeOperation(MOVE, i, moveDone, oppFlagX, oppFlagY);
                        }
                    }
                }
            }

            else if (i == flagDefender) {
                pair<int, int> reachPoint = giveBestCell(myFlagX, myFlagY);
                if (!askSameLine(xx, yy, myFlagX, myFlagY, false, -1,
                                 last)) {  // you are not even in the same line as the flag
                    takeOperation(MOVE, i, moveDone, reachPoint.first, reachPoint.second);
                }

                // I am guaranteed to be in the same line as the flag now

                if (myFlagCarrier == -1) {
                    // flag secure now
                    // attack if anyone is coming
                    if (a1.second > 0) {  // some people are in the same line as me and the flag
                        if (myScore >= powerups[0].price && a1.first == 0)
                            takeOperation(FIRE, i, moveDone);
                        else takeOperation(MOVE, i, moveDone, reachPoint.first, reachPoint.second);
                    } else {  // nobody coming, just chill
                        takeOperation(MOVE, i, moveDone, reachPoint.first, reachPoint.second);
                    }
                }

                else {
                    // flag being taken
                    if (a1.second > 0 && askSameLine(xx, yy, myFlagX, myFlagY, false, -1, last) &&
                        myScore >= min(powerups[0].price, powerups[1].price)) {
                        if (oppFlagCarrier == -1) {
                            if (myScore >= powerups[0].price && a2.first == 0)
                                takeOperation(FIRE, i, moveDone);
                            else takeOperation(MOVE, i, moveDone, oppFlagX, oppFlagY);
                        } else if (distFromMyBase[oppFlagX][oppFlagY] <=
                                       distFromMyBase[oppFlagBaseX][oppFlagBaseY] / 2 &&
                                   myScore >= powerups[1].price) {
                            // flag carrier frozen থাকলে myFlagCarrier != -1 হইতো না
                            takeOperation(FREEZE, i, moveDone);
                        } else if (myScore >= powerups[0].price && a1.first == 0) {
                            takeOperation(FIRE, i, moveDone);
                        } else takeOperation(MOVE, i, moveDone, myFlagX, myFlagY);
                    } else takeOperation(MOVE, i, moveDone, myFlagX, myFlagY);
                }
            }

            else {
                // explorer minion
                // cerr << "Assigned Coin: " << assignedCoins[i].posX << " " <<
                // assignedCoins[i].posY
                //      << endl;
                if (a1.second > 0 && myFlagCarrier != -1 &&
                    askSameLine(xx, yy, myFlagX, myFlagY, false, -1, last)) {
                    if (myScore >= powerups[0].price && a1.first == 0)
                        takeOperation(FIRE, i, moveDone);
                }
                if (myFlagCarrier != -1 && flagDefender != -1 &&
                    askSameLine(xx, yy, myMinions[flagDefender].posX, myMinions[flagDefender].posY,
                                false, -1, last)) {
                    if (movX != -1) takeOperation(MOVE, i, moveDone, movX, movY);
                }
            }
            if (!firstTime && movX != -1) {
                if (last.myMinions[i].health > myMinions[i].health || last.myMinions[i].timeout > 0)
                    takeOperation(MOVE, i, moveDone, movX, movY);
            }
            if (!moveDone) {
                int assigned_exploration_idx =
                    assignedExploration[i];  //একে কোন সেট explore করতে বলা হইসে
                if (!reachedFirstCell[i]) {
                    // this is done to ensure that the minions are spread out
                    // cerr << "Not reached" << endl;
                    takeOperation(MOVE, i, moveDone, firstAssignment[i].first,
                                  firstAssignment[i].second);
                }
                pair<int, int> dest = {-1, -1};
                //আগে দেখো visible coin আছে নাকি?
                if (assignedCoins[i].posX != -1) {  //একটা coin assign করা আছে
                    // cerr << "This?" << endl;
                    dest = {assignedCoins[i].posX, assignedCoins[i].posY};
                }
                if (visibleCoinCnt > 0) {
                    // cerr << "Doing" << endl;
                    int closestDist = INF, closestOne = -1;
                    if (dest.first != -1) {
                        closestDist = abs(dest.first - xx) + abs(dest.second - yy);
                    }
                    for (int k = 0; k < visibleCoinCnt; k++) {
                        if (visibleCoins[k].taken ||
                            !askSameLine(xx, yy, visibleCoins[k].posX, visibleCoins[k].posY, false,
                                         -1, last))
                            continue;
                        // cerr << "Hey" << endl;
                        // for (int l = 0; l < n_minions; l++) {
                        //     if (assignedCoins[l].posX == -1) continue;
                        //     if (askSameLine(assignedCoins[l].posX, assignedCoins[l].posY,
                        //                     visibleCoins[k].posX, visibleCoins[k].posY,
                        //                     false, -1, last))
                        //         continue;
                        // }
                        if (abs(visibleCoins[k].posX - xx) + abs(visibleCoins[k].posY - yy) <
                            closestDist) {
                            closestOne = k;
                            closestDist =
                                abs(visibleCoins[k].posX - xx) + abs(visibleCoins[k].posY - yy);
                        }
                    }
                    if (closestOne != -1) {
                        if (dest.first != -1 && (visibleCoins[closestOne].posX != dest.first ||
                                                 visibleCoins[closestOne].posY != dest.second)) {
                            for (int j = 0; j < visibleCoinCnt; j++) {
                                if (visibleCoins[j].posX == dest.first &&
                                    visibleCoins[j].posY == dest.second) {
                                    visibleCoins[j].taken = false;
                                    break;
                                }
                            }
                        }
                        dest = {visibleCoins[closestOne].posX, visibleCoins[closestOne].posY};
                        visibleCoins[closestOne].taken = true;
                        assignedCoins[i].posX = dest.first;
                        assignedCoins[i].posY = dest.second;
                    }
                    cerr << "Destination: " << dest.first << " " << dest.second << endl;
                }
                if (dest.first == -1) {
                    // cerr << i << " " <<
                    // cellsToExplore[assigned_exploration_idx].size()
                    // << endl;
                    if (!cellsToExplore[assigned_exploration_idx].empty()) {
                        assert(!cellsToExplore[assigned_exploration_idx].empty());
                        while (!cellsToExplore[assigned_exploration_idx].empty()) {
                            pair<int, int> p = cellsToExplore[assigned_exploration_idx].back();
                            if (leftToVisit[p.first][p.second]) {
                                dest = p;
                                break;
                            } else cellsToExplore[assigned_exploration_idx].pop_back();
                        }
                    }
                    if (cellsToExplore[assigned_exploration_idx].empty() &&
                        dest == make_pair(-1, -1)) {
                        int cnt = 0;
                        while (true) {
                            cnt++;
                            if (cnt == n_minions) break;
                            assigned_exploration_idx++;
                            if (assigned_exploration_idx == n_minions) assigned_exploration_idx = 0;
                            if (!cellsToExplore[assigned_exploration_idx].empty()) break;
                        }
                        if (cnt == n_minions) {  //সব empty হয়ে গেসে
                            cerr << "very less probability of this happening though" << endl;
                            if (myFlagCarrier != -1) dest = {myFlagX, myFlagY};
                            else dest = {myFlagBaseX, myFlagBaseY};
                        }
                    }
                }
                takeOperation(MOVE, i, moveDone, dest.first, dest.second);
            }
        }
    }
};

Game last, last2;

int main() {
    readGrid();
    readBases();
    readPowerUps();

    bfs({myFlagBaseX, myFlagBaseY}, distFromMyBase);
    bfs({oppFlagBaseX, oppFlagBaseY}, distFromOppBase);

    // explorer minions will just explore for more and more coins
    init();

    while (true) {
        Game cur;
        cur.readScores();
        cur.readFlag(true);
        cur.readFlag(false);
        cur.readMyMinions(last, last2);
        cur.readOppMinions();
        cur.readCoins();

        cur.doThings(last, last2);

        cur.printOperations();
        firstTime = false;
        last2 = last;
        last = cur;
    }
}