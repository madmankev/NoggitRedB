// This file is part of Noggit3, licensed under GNU General Public License (version 3).

#include "ActionManager.hpp"
#include <noggit/MapView.h>
#include <cmath>

using namespace Noggit;

std::deque<Action*>* ActionManager::getActionStack()
{
  return &_action_stack;
}

Action* ActionManager::getCurrentAction() const
{
  return _cur_action;
}

void ActionManager::setCurrentAction(unsigned index)
{
  int index_diff = index - _undo_index;

  if (!index_diff)
    return;

  if (index_diff > 0)
  {
    for (int i = 0; i < index_diff; ++i)
      undo();
  }
  else
  {
    for (int i = 0; i < std::abs(index_diff); ++i)
      redo();
  }
}


void ActionManager::setLimit(unsigned limit)
{
  _limit = limit;
}

unsigned ActionManager::limit() const
{
  return _limit;
}

void ActionManager::purge()
{
  // Fix for issue #1: purging used to delete the currently running action
  // too, leaving _cur_action dangling. The next endAction()/register call
  // then operated on freed memory and corrupted the undo stack.
  for (auto& action : _action_stack)
  {
    if (action != _cur_action)
    {
      delete action;
    }
  }

  _action_stack.clear();

  if (_cur_action)
  {
    _action_stack.push_back(_cur_action);
  }

  _undo_index = 0;
  emit purged();
}

void ActionManager::dropActionsForTile(MapTile const* tile)
{
  if (!tile || _action_stack.empty())
  {
    return;
  }

  // The undone actions sit at the back of the stack, remember how many of
  // them get erased so _undo_index stays consistent.
  std::size_t const total = _action_stack.size();
  std::size_t const first_undone = total - std::min<std::size_t>(_undo_index, total);

  std::size_t index = 0;
  std::size_t undone_erased = 0;
  bool erased = false;

  for (auto it = _action_stack.begin(); it != _action_stack.end();)
  {
    // Never delete the currently running action from under the tool that is
    // using it, its chunk pointers are about to be invalidated either way
    // once the tile is gone, but deleting it here would crash the editor
    // immediately instead of on the next undo.
    if (*it != _cur_action && (*it)->referencesTile(tile))
    {
      if (index >= first_undone)
      {
        ++undone_erased;
      }

      delete *it;
      it = _action_stack.erase(it);
      erased = true;
    }
    else
    {
      ++it;
    }

    ++index;
  }

  if (erased)
  {
    _undo_index -= static_cast<unsigned>(undone_erased);
    emit actionsInvalidated();
    emit currentActionChanged(_undo_index);
  }
}

Action* ActionManager::beginAction(MapView* map_view
                                   , int flags
                                   , int modality_controls)
{
  if (_cur_action)
    return _cur_action;

  if (!(flags & eDO_NOT_WRITE_HISTORY))
  {
    // clean canceled actions
    if (_undo_index)
    {
      for (unsigned i = 0; i < _undo_index; ++i)
      {
        delete _action_stack.back();
        _action_stack.pop_back();
        emit popBack();
      }
      _undo_index = 0;
    }

    // prevent undo stack overflow
    if (_action_stack.size() == _limit)
    {
      Action* old_action = _action_stack.front();
      delete old_action;
      _action_stack.pop_front();
      emit popFront();
    }
  }

  auto action = new Action(map_view);
  _action_stack.push_back(action);

  action->setFlags(flags);
  action->setModalityControllers(modality_controls);

  _cur_action = action;

  emit onActionBegin(action);

  return action;
}

void ActionManager::endAction()
{
  assert(_cur_action && "ActionStack Error: endAction() called with no action running.");

  _cur_action->finish();
  if (!(_cur_action->getFlags() & eDO_NOT_WRITE_HISTORY))
  {
    emit addedAction(_cur_action);
    emit onActionEnd(_cur_action);
  }
  else
  {
    // non-history actions are removed from the stack, delete them as well
    // instead of leaking them (and emitting with a dangling pointer).
    Action* action = _cur_action;
    emit onActionEnd(action);
    _action_stack.pop_back();
    delete action;
  }

  _cur_action = nullptr;
  emit currentActionChanged(_undo_index);
}

void ActionManager::endActionOnModalityMismatch(unsigned modality_controls)
{
  if (!_cur_action)
    return;

  if (!_cur_action->getModalityControllers())
    return;

  if ((modality_controls & _cur_action->getModalityControllers()) != _cur_action->getModalityControllers())
  {
    _cur_action->finish();
    emit onActionEnd(_cur_action);
    if (!(_cur_action->getFlags() & eDO_NOT_WRITE_HISTORY))
    {
      emit addedAction(_cur_action);
    }
    else
    {
      // see endAction(): popped non-history actions must be deleted too.
      _action_stack.pop_back();
      delete _cur_action;
    }
    _cur_action = nullptr;
    emit currentActionChanged(_undo_index);
  }
}

void ActionManager::undo()
{
  assert(!_cur_action && "ActionStack Error: undo initiated while action is running.");

  if (_action_stack.empty())
    return;

  int index = static_cast<int>(_action_stack.size()) - static_cast<int>(_undo_index) - 1;

  if (index < 0)
    return;

  Action* action = _action_stack.at(index);
  action->undo();

  _undo_index++;
  emit currentActionChanged(_undo_index);
}

void ActionManager::redo()
{
  assert(!_cur_action && "ActionStack Error: redo initiated while action is running.");

  if (_action_stack.empty())
    return;

  if (!_undo_index)
    return;

  unsigned index = static_cast<int>(_action_stack.size()) - static_cast<int>(_undo_index);

  Action* action = _action_stack.at(index);
  action->undo(true);

  _undo_index--;
  emit currentActionChanged(_undo_index);
}

ActionManager::~ActionManager()
{
  for (auto& action : _action_stack)
  {
    delete action;
  }

  _action_stack.clear();
}
