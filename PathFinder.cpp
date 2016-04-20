/****************************************************************************
**
** PathFinder.cpp
**
** Copyright (C) October 2015 Hotride
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
*****************************************************************************
*/
//---------------------------------------------------------------------------
#include "stdafx.h"

TPathFinder *PathFinder = NULL;
//---------------------------------------------------------------------------
//--------------------------------TPathObject--------------------------------
//---------------------------------------------------------------------------
TPathObject::TPathObject(int z, char height, char surface)
: TBaseQueueItem(), m_Z(z), m_Height(height), m_Surface(surface)
{
}
//---------------------------------------------------------------------------
TPathObject::~TPathObject()
{
}
//---------------------------------------------------------------------------
//---------------------------------TPathNode---------------------------------
//---------------------------------------------------------------------------
TPathNode::TPathNode()
: m_Parent(NULL), m_X(0), m_Y(0), m_Z(0), m_Used(false), m_Cost(0),
m_Direction(0), m_DistFromStartCost(0), m_DistFromGoalCost(0)
{
}
//---------------------------------------------------------------------------
TPathNode::~TPathNode()
{
	m_Parent = NULL;
}
//---------------------------------------------------------------------------
void TPathNode::Reset()
{
	m_Parent = NULL;
	m_Used = false;
	m_X = m_Y = m_Z = m_Direction = m_Cost = m_DistFromStartCost = m_DistFromGoalCost = 0;
}
//---------------------------------------------------------------------------
//---------------------------------TPathFinder-------------------------------
//---------------------------------------------------------------------------
TPathFinder::TPathFinder()
: TBaseQueue(), m_OnLongStair(false), m_AutoWalking(false), m_PointIndex(0),
m_PathSize(0), /*m_StartX(0), m_StartY(0), m_EndX(0), m_EndY(0),*/ m_GoalNode(0),
m_GoalFound(false), m_ActiveOpenNodes(0), m_ActiveClosedNodes(0),
m_PathFindDistance(0), m_PathFindidngCanBeCancelled(false)
{
	m_StartPoint.x = 0;
	m_StartPoint.y = 0;

	m_EndPoint.x = 0;
	m_EndPoint.y = 0;
}
//---------------------------------------------------------------------------
TPathFinder::~TPathFinder()
{
}
//---------------------------------------------------------------------------
bool TPathFinder::CreateItemsList(int &x, int &y)
{
	Clear();

	int blockX = x / 8;
	int blockY = y / 8;

	int blockIndex = (blockX * g_MapBlockY[g_CurrentMap]) + blockY;

	TMapBlock *block = MapManager->GetBlock(blockIndex);

	if (block == NULL)
	{
		block = MapManager->AddBlock(blockIndex);
		block->X = blockX;
		block->Y = blockY;
		MapManager->LoadBlock(block);
		//return false;
	}

	int bx = x % 8;
	int by = y % 8;

	bool ignoreGameObjects = (g_Player->Graphic == 0x03DB);
	bool ignoreDoors = g_Player->Dead();
	bool ignoreGameCharacters = (g_Player->Stam >= g_Player->MaxStam || g_Player->Dead());

	TRenderWorldObject *obj = block->GetRender(bx, by);

	while (obj != NULL)
	{
		WORD graphic = obj->Graphic;

		if (obj->IsLandObject())
		{
			if (graphic != 2)
			{
				DWORD flags = UO->GetLandFlags(graphic);

				char surface = 0x10 + (char)(!IsImpassable(flags));

				Add(new TPathObject(obj->Z, 0, surface));
			}
		}
		else if (obj->IsStaticGroupObject())
		{
			bool canBeAdd = true;

			if (obj->IsGameObject())
			{
				if (ignoreGameObjects || graphic >= 0x4000 || (ignoreDoors && obj->IsDoor()) || obj->IsInternal()) //GM || isMulti || (ghost && isDoor) || InternalItem
					canBeAdd = false;
				else if (((TGameObject*)obj)->NPC)
				{
					if (!ignoreGameCharacters)
						Add(new TPathObject(obj->Z, DEFAULT_CHARACTER_HEIGHT, 0));

					canBeAdd = false;
				}
			}
			else
				graphic -= 0x4000;

			if (canBeAdd)
			{
				bool impSurf = (graphic > 1 && (obj->IsImpassable() || obj->IsSurface() || obj->IsBridge()));

				//if (graphic == 1 || !impSurf || (obj->IsBackground() && !impSurf)) {} else

				if (impSurf)
				{
					BYTE height = ((TRenderStaticObject*)obj)->GetStaticHeight();

					char surface = (char)(obj->IsSurface() && !obj->IsImpassable());

					if (obj->IsBridge() && obj->IsPrefixA() && height >= 10) //long stair
						surface += 0x20;

					if (!height && obj->IsBackground() && obj->IsSurface() && obj->Z >= g_Player->Z + 3)
						surface += 0x40;

					Add(new TPathObject(obj->Z, height, surface));
				}
			}
		}

		obj = obj->m_NextXY;
	}

	return true;
}
//---------------------------------------------------------------------------
void TPathFinder::CheckLongStairUnderfoot(int &x, int &y, char &z)
{
	m_OnLongStair = false;

	if (CreateItemsList(x, y))
	{
		QFOR(po, m_Items, TPathObject*)
		{
			char surface = po->Surface;
			int isLongStair = 1;
			
			if (surface >= 0x40)
				surface -= 0x40;

			if (surface >= 0x20)
			{
				surface -= 0x20;
				isLongStair++;
			}

			if (surface > 1)
				surface -= 0x10;

			if (surface)
			{
				char top = po->Z + (char)(po->Height / isLongStair);

				if (top == z)
					m_OnLongStair = (isLongStair > 1);
			}
		}

		Clear();
	}
}
//---------------------------------------------------------------------------
bool TPathFinder::CalculateNewZ(int &x, int &y, char &z)
{
	if (!CreateItemsList(x, y))
		return false;
	
	z = g_Player->Z;
	
	int newZ = z;

	if (m_Items != NULL)
	{
		TPathObject *po = (TPathObject*)m_Items;

		while (po != NULL && po->m_Next != NULL)
			po = (TPathObject*)po->m_Next;

		TPathObject *poStart = po;
		bool found = false;

		int c = 0;
		for (; po != NULL; po = (TPathObject*)po->m_Prev)
		{
			c++;
			int height = po->Height;
			int top = po->Z + height;
			char surface = po->Surface;
			bool isLand = false;
			bool isLongStair = false;
			
			if (surface >= 0x40)
			{
				surface -= 0x40;
				height = 3;
			}

			if (surface >= 0x20)
			{
				surface -= 0x20;
				isLongStair = true;
			}

			if (surface > 1)
			{
				surface -= 0x10;
				isLand = true;
			}

			if (found && (top > (newZ - g_MaxBlockZ) || !surface))
				break;

			if (!surface && top < newZ) // top <= newZ
			{
				found = false;
				break;
			}

			if (surface)
			{
				if (top < newZ + g_MaxClimbZ + (isLongStair ? height : 0) && top >= newZ - g_MaxFallZ)
				{
					if (found && abs(z - newZ) <= abs(z - top)) // <
						break;
					
					if (isLongStair && top > newZ + g_MaxClimbZ)
						newZ = po->Z + (height / 2);
					else
						newZ = top;
					
					//TPRINT("f1::Found_1::newZ=%i (top=%i)\n", newZ, top);
					found = true;

					if (height > 1)
						break;
				}
				else if ((isLand && top <= newZ + g_MaxClimbMapZ && top >= newZ - g_MaxFallZ) || (top < newZ))
				{
					if (found && abs(z - newZ) < abs(z - top))
						break;
					
					//TPRINT("f1::Found_2::newZ=%i\n", newZ);
					newZ = top;
					found = true;
				}
			}
		}

		if (!found)
		{
			//TPRINT("Failed 1\n");
			Clear();

			return false;
		}

		//found = false;

		//TPRINT("c=%i\n", c);
		c = 0;
		
		for (po = poStart; po != NULL; po = (TPathObject*)po->m_Prev)
		{
			c++;
			int curZ = po->Z;
			int top = curZ + po->Height;
			char surface = po->Surface;
			bool isLongStair = false;
			
			if (surface >= 0x40)
			{
				surface -= 0x40;
				top += 3;
			}

			if (surface >= 0x20)
			{
				surface -= 0x20;
				isLongStair = (curZ <= (z + 6));
			}

			//TPRINT("surface=%i nowC=%i z=%i newZ=%i curZ=%i top=%i isLongStair=%i\n", surface, c, z, newZ, curZ, top, isLongStair);

			if (top <= newZ)
				continue;
			
			if (surface < 0x10 && !isLongStair)
			//if (!surface)
			{
				if (top > newZ && top < (newZ + g_MaxBlockZ))
				{
					//TPRINT("f2_1\n");
					found = false;

					break;
				}
				else if (curZ >= z && curZ < (z + (g_MaxBlockZ / 2)))
				{
					//TPRINT("f2_2\n");
					found = false;

					break;
				}
				else if (curZ <= z && top >= (z + (g_MaxBlockZ / 2)))
				{
					//TPRINT("f2_3\n");
					found = false;

					break;
				}
				else if (curZ >= newZ && curZ <= (newZ + g_MaxBlockZ))
				{
					//TPRINT("f2_4\n");
					found = false;

					break;
				}
			}
		}

		if (!found)
		{
			//TPRINT("Failed 2\n");
			Clear();

			return false;
		}
	}

	z = newZ;

	return true;
}
//---------------------------------------------------------------------------
void TPathFinder::GetNewXY(BYTE &direction, int &x, int &y)
{
	switch (direction & 7)
	{
		case 0:
		{
			y--;
			break;
		}
		case 1:
		{
			x++;
			y--;
			break;
		}
		case 2:
		{
			x++;
			break;
		}
		case 3:
		{
			x++;
			y++;
			break;
		}
		case 4:
		{
			y++;
			break;
		}
		case 5:
		{
			x--;
			y++;
			break;
		}
		case 6:
		{
			x--;
			break;
		}
		case 7:
		{
			x--;
			y--;
			break;
		}
	}
}
//---------------------------------------------------------------------------
bool TPathFinder::CanWalk(BYTE &direction, int &x, int &y, char &z)
{
	int newX = x;
	int newY = y;
	char newZ = z;
	BYTE newDirection = direction;

	CheckLongStairUnderfoot(x, y, z);

	GetNewXY(direction, newX, newY);

	bool passed = CalculateNewZ(newX, newY, newZ);

	if ((char)direction % 2) //diagonal
	{
		const char dirOffset[2] = {1, -1};

		if (passed) //test angleowner tiles
		{
			IFOR(i, 0, 2 && passed)
			{
				int testX = x;
				int testY = y;
				char testZ = z;
		
				BYTE testDir = (direction + dirOffset[i]) % 8;
				GetNewXY(testDir, testX, testY);

				passed = CalculateNewZ(testX, testY, testZ);
			}
		}

		if (!passed) //test neary tiles
		{
			IFOR(i, 0, 2 && !passed)
			{
				newX = x;
				newY = y;
				newZ = z;

				newDirection = (direction + dirOffset[i]) % 8;
				GetNewXY(newDirection, newX, newY);
				
				passed = CalculateNewZ(newX, newY, newZ);
			}
		}
	}

	if (passed)
	{
		x = newX;
		y = newY;
		z = newZ;
		direction = newDirection;
	}

	return passed;
}
//---------------------------------------------------------------------------
bool TPathFinder::Walk(bool run, BYTE direction)
{
	if (g_LastStepTime > GetTickCount() || g_WalkRequestCount > 1 || g_Player == NULL || g_DeathScreenTimer)
		return false;

	if (!run)
		run = ConfigManager.AlwaysRun;

	int x = g_Player->X;
	int y = g_Player->Y;
	char z = g_Player->Z;
	BYTE olddir = g_Player->Direction;

	bool emptyStack = true;
	TWalkData *walker = g_Player->m_WalkStack.Top();
	if (walker != NULL)
	{
		x = walker->X;
		y = walker->Y;
		z = walker->Z;
		olddir = walker->Direction;
		emptyStack = false;
	}

	WALKER_SEND_ITEM wsi;
	wsi.Time = 100;

	if ((olddir & 7) == (direction & 7)) //��������� ���� ����
	{
		BYTE newDir = direction;
		int newX = x;
		int newY = y;
		char newZ = z;

		if (!CanWalk(newDir, newX, newY, newZ))
			return false;

		if ((direction & 7) != newDir)
		{
			WALKER_SEND_ITEM correctDirWSI = {newDir, 100};
			
			TWalkData *correctDirWD = new TWalkData();
			correctDirWD->X = x;
			correctDirWD->Y = y;
			correctDirWD->Z = z;
			correctDirWD->Direction = direction;

			g_Player->m_WalkStack.Push(correctDirWD);

			Walker->m_SendStack.push_back(correctDirWSI);
		}
		
		direction = newDir;
		x = newX;
		y = newY;
		z = newZ;

		bool onMount = (g_Player->FindLayer(OL_MOUNT) != NULL);

		wsi.Time = CHARACTER_ANIMATION_DELAY_TABLE[onMount][run];
	}
	else
	{
		BYTE newDir = direction;
		int newX = x;
		int newY = y;
		char newZ = z;

		if (!CanWalk(newDir, newX, newY, newZ))
		{
			if ((olddir & 7) == newDir)
				return false;
		}

		if ((olddir & 7) == newDir)
		{
			x = newX;
			y = newY;
			z = newZ;

			bool onMount = (g_Player->FindLayer(OL_MOUNT) != NULL);

			wsi.Time = CHARACTER_ANIMATION_DELAY_TABLE[onMount][run];
		}

		direction = newDir;
	}

	TGameItem *bank = g_Player->FindLayer(OL_BANK);

	if (bank != NULL)
	{
		TGump *bankContainer = GumpManager->GetGump(bank->Serial, 0, GT_CONTAINER);

		if (bankContainer != NULL)
			GumpManager->RemoveGump(bankContainer);
	}

	if (run)
		direction += 0x80;

	wsi.Dir = direction;

	TWalkData *wd = new TWalkData();
	wd->X = x;
	wd->Y = y;
	wd->Z = z;
	wd->Direction = direction;

	g_RemoveRangeXY.x = x;
	g_RemoveRangeXY.y = y;

	g_UpdateRange--;

	UO->RemoveRangedObjects();

	g_UpdateRange++;

	if (emptyStack)
	{
		if (!g_Player->Walking())
			g_Player->SetAnimation(0xFF);

		g_Player->LastStepTime = GetTickCount();
	}

	g_Player->m_WalkStack.Push(wd);

	World->MoveToTop(g_Player);

	Walker->m_SendStack.push_back(wsi);
	g_Player->GetAnimationGroup();

	return true;
}
//---------------------------------------------------------------------------
int TPathFinder::GetGoalDistCost(const POINT &p, int cost)
{
	return (abs(m_EndPoint.x - p.x) + abs(m_EndPoint.y - p.y)) * cost;
}
//---------------------------------------------------------------------------
bool TPathFinder::DoesNotExistOnOpenList(int x, int y)
{
	bool result = false;

	IFOR(i, 0, PATHFINDER_MAX_NODES)
	{
		if (m_OpenList[i].Used &&m_OpenList[i].X == x && m_OpenList[i].Y == y)
		{
			result = true;
			break;
		}
	}

	return result;
}
//---------------------------------------------------------------------------
bool TPathFinder::DoesNotExistOnClosedList(int x, int y)
{
	bool result = false;

	IFOR(i, 0, PATHFINDER_MAX_NODES)
	{
		if (m_ClosedList[i].Used && m_ClosedList[i].X == x && m_ClosedList[i].Y == y)
		{
			result = true;
			break;
		}
	}

	return result;
}
//---------------------------------------------------------------------------
int TPathFinder::AddNodeToList(int list, int direction, int x, int y, int z, TPathNode *parentNode, int cost)
{
	if (!list)
	{
		if (!DoesNotExistOnClosedList(x, y))
		{
			if (!DoesNotExistOnOpenList(x, y))
			{
				IFOR(i, 0, PATHFINDER_MAX_NODES)
				{
					if (!m_OpenList[i].Used)
					{
						m_OpenList[i].Used = true;

						m_OpenList[i].Direction = direction;
						m_OpenList[i].X = x;
						m_OpenList[i].Y = y;
						m_OpenList[i].Z = z;
						POINT p = { x, y };

						m_OpenList[i].DistFromGoalCost = GetGoalDistCost(p, cost);
						m_OpenList[i].DistFromStartCost = parentNode->DistFromStartCost + cost;
						m_OpenList[i].Cost = m_OpenList[i].DistFromGoalCost + m_OpenList[i].DistFromStartCost;

						m_OpenList[i].Parent = parentNode;

						//if (x == m_EndX && y == m_EndY)
						if (GetDistance(m_EndPoint, p) <= m_PathFindDistance)
						{
							m_GoalFound = true;
							m_GoalNode = i;
						}

						m_ActiveOpenNodes++;

						return i;
					}
				}

				return -1;
			}
			else
			{
				IFOR(i, 0, PATHFINDER_MAX_NODES)
				{
					if (m_OpenList[i].Used)
					{
						if (m_OpenList[i].X == x && m_OpenList[i].Y == y)
						{
							int startCost = parentNode->DistFromStartCost + cost;

							if (m_OpenList[i].DistFromStartCost > startCost)
							{
								m_OpenList[i].Parent = parentNode;
								m_OpenList[i].DistFromStartCost = startCost + cost;
								m_OpenList[i].Cost = m_OpenList[i].DistFromGoalCost + m_OpenList[i].DistFromStartCost;
							}

							return i;
						}
					}
				}
			}
		}
		else
			return 0;
	}
	else
	{
		parentNode->Used = false;

		IFOR(i, 0, PATHFINDER_MAX_NODES)
		{
			if (!m_ClosedList[i].Used)
			{
				m_ClosedList[i].Used = true;

				m_ClosedList[i].DistFromGoalCost = parentNode->DistFromGoalCost;
				m_ClosedList[i].DistFromStartCost = parentNode->DistFromStartCost;
				m_ClosedList[i].Cost = m_ClosedList[i].DistFromGoalCost + m_ClosedList[i].DistFromStartCost;
				m_ClosedList[i].Direction = parentNode->Direction;
				m_ClosedList[i].X = parentNode->X;
				m_ClosedList[i].Y = parentNode->Y;
				m_ClosedList[i].Z = parentNode->Z;
				m_ClosedList[i].Parent = parentNode->Parent;

				m_ActiveOpenNodes--;
				m_ActiveClosedNodes++;

				return i;
			}
		}

		return -1;
	}

	return -1;
}
//---------------------------------------------------------------------------
bool TPathFinder::OpenNodes(TPathNode *node)
{
	bool found = false;

	IFOR(i, 0, 8)
	{
		BYTE direction = (BYTE)i;
		int x = node->X;
		int y = node->Y;
		char z = (char)node->Z;

		BYTE oldDirection = direction;

		if (CanWalk(direction, x, y, z))
		{
			if (direction != oldDirection)
				continue;

			int diagonal = i % 2;

			if (diagonal)
			{
				BYTE wantDirection = (BYTE)i;
				int wantX = node->X;
				int wantY = node->Y;

				GetNewXY(wantDirection, wantX, wantY);

				if (x != wantX || y != wantY)
					diagonal = -1;
			}

			if (diagonal >= 0 && AddNodeToList(0, direction, x, y, z, node, 1 + diagonal) != -1)
				found = true;
			else
				continue;
		}
	}

	return found;
}
//---------------------------------------------------------------------------
int TPathFinder::FindCheapestNode()
{
	int cheapestCost = 9999999;
	int cheapestNode = -1;

	IFOR(i, 0, PATHFINDER_MAX_NODES)
	{
		if (m_OpenList[i].Used)
		{
			if (m_OpenList[i].Cost < cheapestCost)
			{
				cheapestNode = i;
				cheapestCost = m_OpenList[i].Cost;
			}
		}
	}

	int result = -1;

	if (cheapestNode != -1)
		result = AddNodeToList(1, 0, 0, 0, 0, &m_OpenList[cheapestNode], 2);

	return result;
}
//---------------------------------------------------------------------------
bool TPathFinder::FindPath(int maxNodes)
{
	int curNode = 0;

	m_ClosedList[0].Used = true;

	m_ClosedList[0].X = m_StartPoint.x;
	m_ClosedList[0].Y = m_StartPoint.y;

	m_ClosedList[0].DistFromGoalCost = GetGoalDistCost(m_StartPoint, 0);
	m_ClosedList[0].Cost = m_ClosedList[0].DistFromGoalCost;

	while (true)
	{
		OpenNodes(&m_ClosedList[curNode]);

		if (m_GoalFound)
		{
			int totalNodes = 0;

			TPathNode *GoalNode = &m_OpenList[m_GoalNode];

			while (GoalNode->Parent != NULL)
			{
				GoalNode = GoalNode->Parent;
				totalNodes++;
			};

			totalNodes++;

			m_PathSize = totalNodes;

			GoalNode = &m_OpenList[m_GoalNode];

			while (totalNodes > 0)
			{
				totalNodes--;
				m_Path[totalNodes] = GoalNode;
				GoalNode = GoalNode->Parent;
			};

			break;
		}

		curNode = FindCheapestNode();

		if (curNode == -1)
			return false;

		if (m_ActiveClosedNodes >= maxNodes)
			return false;
	}

	return true;
}
//---------------------------------------------------------------------------
bool TPathFinder::WalkTo(int x, int y, int z, int distance)
{
	IFOR(i, 0, m_ActiveOpenNodes)
		m_OpenList[i].Reset();

	IFOR(i, 0, m_ActiveClosedNodes)
		m_ClosedList[i].Reset();

	m_StartPoint.x = g_Player->X;
	m_StartPoint.y = g_Player->Y;
	m_EndPoint.x = x;
	m_EndPoint.y = y;
	m_GoalNode = 0;
	m_GoalFound = false;
	m_ActiveOpenNodes = 0;
	m_ActiveClosedNodes = 0;
	m_PathFindDistance = distance;
	m_PathSize = 0;
	m_PathFindidngCanBeCancelled = true;

	StopAutoWalk();

	if (FindPath(PATHFINDER_MAX_NODES))
	{
		m_AutoWalking = true;
		m_PointIndex = 1;
		ProcessAutowalk();
	}

	return (m_PathSize != 0);
}
//---------------------------------------------------------------------------
void TPathFinder::ProcessAutowalk()
{
	if (m_AutoWalking && g_Player != NULL && !g_DeathScreenTimer && g_WalkRequestCount <= 1 && g_LastStepTime <= GetTickCount())
	{
		if (m_PointIndex >= 0 && m_PointIndex < m_PathSize)
		{
			TPathNode *p = m_Path[m_PointIndex];

			BYTE olddir = g_Player->Direction;

			TWalkData *walker = g_Player->m_WalkStack.Top();
			if (walker != NULL)
				olddir = walker->Direction;

			if ((olddir & 7) == p->Direction)
				m_PointIndex++;

			if (!Walk(ConfigManager.AlwaysRun, p->Direction))
				StopAutoWalk();
		}
		else
			StopAutoWalk();
	}
}
//---------------------------------------------------------------------------
void TPathFinder::StopAutoWalk()
{
	m_AutoWalking = false;
	m_PathSize = 0;
}
//---------------------------------------------------------------------------