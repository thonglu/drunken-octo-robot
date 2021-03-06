/*
 * world.cpp
 *
 *  Created on: 2012-05-14
 *      Author: dri
 */


#include "../p3util/cOnscreenText.h"
#include "pandaFramework.h"
#include "ambientLight.h"
#include "directionalLight.h"
#include "auto_bind.h"
#include "world.h"

const float SPEED = 0.5; // Note: unused

// Function to put instructions on the screen.
NodePath World::add_instructions(float pos, const string& msg) const
   {
   COnscreenText instructions("instructions", COnscreenText::TS_plain);
   instructions.set_text(msg);
   instructions.set_fg(Colorf(1,1,1,1));
   instructions.set_pos(LVecBase2f(-1.3, pos));
   instructions.set_align(TextNode::A_left);
   instructions.set_scale(0.05);
   instructions.reparent_to(m_windowFrameworkPtr->get_aspect_2d());
   return instructions.generate();
   }

// Function to put title on the screen.
NodePath World::add_title(const string& text) const
   {
   COnscreenText title("title", COnscreenText::TS_plain);
   title.set_text(text);
   title.set_fg(Colorf(1,1,1,1));
   title.set_pos(LVecBase2f(1.3,-0.95));
   title.set_align(TextNode::A_right);
   title.set_scale(0.07);
   title.reparent_to(m_windowFrameworkPtr->get_aspect_2d());
   return title.generate();
   }

World::World(WindowFramework* windowFrameworkPtr)
   : m_windowFrameworkPtr(windowFrameworkPtr)
   {
   // preconditions
   if(m_windowFrameworkPtr == NULL)
      {
      nout << "ERROR: World::World(WindowFramework* windowFrameworkPtr) parameter windowFrameworkPtr cannot be NULL." << endl;
      return;
      }

   m_keyMap.resize(K_keys);
   m_keyMap[K_left     ] = false;
   m_keyMap[K_right    ] = false;
   m_keyMap[K_forward  ] = false;
   m_keyMap[K_cam_left ] = false;
   m_keyMap[K_cam_right] = false;
   m_windowFrameworkPtr->set_background_type(WindowFramework::BT_black);

   // Post the instructions

   m_titleNp = add_title("Panda3D Tutorial: Roaming Ralph (Walking on Uneven Terrain)");
   m_inst1Np = add_instructions(0.95, "[ESC]: Quit");
   m_inst2Np = add_instructions(0.90, "[Left Arrow]: Rotate Ralph Left");
   m_inst3Np = add_instructions(0.85, "[Right Arrow]: Rotate Ralph Right");
   m_inst4Np = add_instructions(0.80, "[Up Arrow]: Run Ralph Forward");
   m_inst6Np = add_instructions(0.70, "[A]: Rotate Camera Left");
   m_inst7Np = add_instructions(0.65, "[S]: Rotate Camera Right");

   // Set up the environment
   //
   // This environment model contains collision meshes.  If you look
   // in the egg file, you will see the following:
   //
   //    <Collide> { Polyset keep descend }
   //
   // This tag causes the following mesh to be converted to a collision
   // mesh -- a mesh which is optimized for collision, not rendering.
   // It also keeps the original mesh, so there are now two copies ---
   // one optimized for rendering, one for collisions.

   NodePath modelsNp = m_windowFrameworkPtr->get_panda_framework()->get_models();
   m_environNp = m_windowFrameworkPtr->load_model(modelsNp, "../models/world");
   NodePath renderNp = m_windowFrameworkPtr->get_render();
   m_environNp.reparent_to(renderNp);
   m_environNp.set_pos(0,0,0);

   // Create the main character, Ralph
   LPoint3f ralphStartPos = m_environNp.find("**/start_point").get_pos();
   CActor::AnimMap ralphAnims;
   ralphAnims["../models/ralph-run"].push_back("run");
   ralphAnims["../models/ralph-walk"].push_back("walk");
   m_ralph.load_actor(m_windowFrameworkPtr,
                      "../models/ralph",
                      &ralphAnims,
                      PartGroup::HMF_ok_wrong_root_name|
                      PartGroup::HMF_ok_anim_extra|
                      PartGroup::HMF_ok_part_extra);
   m_ralph.reparent_to(renderNp);
   m_ralph.set_scale(0.2);
   m_ralph.set_pos(ralphStartPos);

   // Create a floater object.  We use the "floater" as a temporary
   // variable in a variety of calculations.

   m_floaterNp = NodePath("floater");
   m_floaterNp.reparent_to(renderNp);

   // Accept the control keys for movement and rotation
   m_windowFrameworkPtr->enable_keyboard();
   m_windowFrameworkPtr->get_panda_framework()->define_key("escape"        , "sysExit"    , sys_exit                        , NULL);
   m_windowFrameworkPtr->get_panda_framework()->define_key("arrow_left"    , "left"       , call_set_key<K_left     , true >, this);
   m_windowFrameworkPtr->get_panda_framework()->define_key("arrow_right"   , "right"      , call_set_key<K_right    , true >, this);
   m_windowFrameworkPtr->get_panda_framework()->define_key("arrow_up"      , "forward"    , call_set_key<K_forward  , true >, this);
   m_windowFrameworkPtr->get_panda_framework()->define_key("a"             , "cam-left"   , call_set_key<K_cam_left , true >, this);
   m_windowFrameworkPtr->get_panda_framework()->define_key("s"             , "cam-right"  , call_set_key<K_cam_right, true >, this);
   m_windowFrameworkPtr->get_panda_framework()->define_key("arrow_left-up" , "leftUp"     , call_set_key<K_left     , false>, this);
   m_windowFrameworkPtr->get_panda_framework()->define_key("arrow_right-up", "rightUp"    , call_set_key<K_right    , false>, this);
   m_windowFrameworkPtr->get_panda_framework()->define_key("arrow_up-up"   , "forwardUp"  , call_set_key<K_forward  , false>, this);
   m_windowFrameworkPtr->get_panda_framework()->define_key("a-up"          , "cam-leftUp" , call_set_key<K_cam_left , false>, this);
   m_windowFrameworkPtr->get_panda_framework()->define_key("s-up"          , "cam-rightUp", call_set_key<K_cam_right, false>, this);

   PT(GenericAsyncTask) taskPtr = new GenericAsyncTask("moveTask", call_move, this);
   if(taskPtr != NULL)
      {
      AsyncTaskManager::get_global_ptr()->add(taskPtr);
      }

   // Game state variables
   m_isMoving = false;

   // Set up the camera

   // Note: no need to disable the mouse in C++
   NodePath cameraNp = m_windowFrameworkPtr->get_camera_group();
   cameraNp.set_pos(m_ralph.get_x(), m_ralph.get_y()+10, 2);

   // We will detect the height of the terrain by creating a collision
   // ray and casting it downward toward the terrain.  One ray will
   // start above ralph's head, and the other will start above the camera.
   // A ray may hit the terrain, or it may hit a rock or a tree.  If it
   // hits the terrain, we can detect the height.  If it hits anything
   // else, we rule that the move is illegal.

   NodePath ralphGroundColNp;
   m_ralphGroundRayPtr = new CollisionRay();
   if(m_ralphGroundRayPtr != NULL)
      {
      m_ralphGroundRayPtr->set_origin(0, 0, 1000);
      m_ralphGroundRayPtr->set_direction(0, 0, -1);
      m_ralphGroundColPtr = new CollisionNode("ralphRay");
      if(m_ralphGroundColPtr != NULL)
         {
         m_ralphGroundColPtr->add_solid(m_ralphGroundRayPtr);
         m_ralphGroundColPtr->set_from_collide_mask(BitMask32::bit(0));
         m_ralphGroundColPtr->set_into_collide_mask(BitMask32::all_off());
         ralphGroundColNp = m_ralph.attach_new_node(m_ralphGroundColPtr);
         m_ralphGroundHandlerPtr = new CollisionHandlerQueue();
         if(m_ralphGroundHandlerPtr != NULL)
            {
            m_collisionTraverser.add_collider(ralphGroundColNp, m_ralphGroundHandlerPtr);
            }
         }
      }

   NodePath camGroundColNp;
   m_camGroundRayPtr = new CollisionRay();
   if(m_camGroundRayPtr != NULL)
      {
      m_camGroundRayPtr->set_origin(0, 0, 1000);
      m_camGroundRayPtr->set_direction(0, 0, -1);
      m_camGroundColPtr = new CollisionNode("camRay");
      if(m_camGroundColPtr != NULL)
         {
         m_camGroundColPtr->add_solid(m_camGroundRayPtr);
         m_camGroundColPtr->set_from_collide_mask(BitMask32::bit(0));
         m_camGroundColPtr->set_into_collide_mask(BitMask32::all_off());
         camGroundColNp = cameraNp.attach_new_node(m_camGroundColPtr);
         m_camGroundHandlerPtr = new CollisionHandlerQueue();
         if(m_camGroundHandlerPtr != NULL)
            {
            m_collisionTraverser.add_collider(camGroundColNp, m_camGroundHandlerPtr);
            }
         }
      }

   // Uncomment this line to see the collision rays
   //ralphGroundColNp.show();
   //camGroundColNp.show();

   // Uncomment this line to show a visual representation of the
   // collisions occuring
   //m_collisionTraverser.show_collisions(renderNp);

   // Create some lighting
   PT(AmbientLight) ambientLightPtr = new AmbientLight("ambientLight");
   if(ambientLightPtr != NULL)
      {
      ambientLightPtr->set_color(Colorf(.3, .3, .3, 1));
      renderNp.set_light(renderNp.attach_new_node(ambientLightPtr));
      }
   PT(DirectionalLight) directionalLightPtr = new DirectionalLight("directionalLightPtr");
   if(directionalLightPtr != NULL)
      {
      directionalLightPtr->set_direction(LVecBase3f(-5, -5, -5));
      directionalLightPtr->set_color(Colorf(1, 1, 1, 1));
      directionalLightPtr->set_specular_color(Colorf(1, 1, 1, 1));
      renderNp.set_light(renderNp.attach_new_node(directionalLightPtr));
      }
   }

// Records the state of the arrow keys
void World::set_key(Key key, bool value)
   {
   if(key < 0 || (unsigned int)key >= m_keyMap.size())
      {
      nout << "void World::set_key(Key key, bool value) parameter key is out of range: " << key << endl;
      }

   m_keyMap[key] = value;
   }

// Accepts arrow keys to move either the player or the menu cursor,
// Also deals with grid checking and collision detection
void World::move()
   {
   // If the camera-left key is pressed, move camera left.
   // If the camera-right key is pressed, move camera right.

   NodePath cameraNp = m_windowFrameworkPtr->get_camera_group();
   cameraNp.look_at(m_ralph);
   if(m_keyMap[K_cam_left] != false)
      {
      cameraNp.set_x(cameraNp, -20 * ClockObject::get_global_clock()->get_dt());
      }
   if(m_keyMap[K_cam_right] != false)
      {
      cameraNp.set_x(cameraNp, +20 * ClockObject::get_global_clock()->get_dt());
      }

   // save ralph's initial position so that we can restore it,
   // in case he falls off the map or runs into something.

   LPoint3f startPos = m_ralph.get_pos();

   // If a move-key is pressed, move ralph in the specified direction.

   if(m_keyMap[K_left])
      {
      m_ralph.set_h(m_ralph.get_h() + 300 * ClockObject::get_global_clock()->get_dt());
      }
   if(m_keyMap[K_right])
      {
      m_ralph.set_h(m_ralph.get_h() - 300 * ClockObject::get_global_clock()->get_dt());
      }
   if(m_keyMap[K_forward])
      {
      m_ralph.set_y(m_ralph, -25 * ClockObject::get_global_clock()->get_dt());
      }

   // If ralph is moving, loop the run animation.
   // If he is standing still, stop the animation.

   if(m_keyMap[K_forward] || m_keyMap[K_left] || m_keyMap[K_right])
      {
       if(!m_isMoving)
          {
          m_ralph.loop("run", true);
          m_isMoving = true;
          }
      }
   else
      {
      if(m_isMoving)
         {
         m_ralph.stop("run");
         m_ralph.pose("walk", 5);
         m_isMoving = false;
         }
      }

   // If the camera is too far from ralph, move it closer.
   // If the camera is too close to ralph, move it farther.

   LPoint3f camVec = m_ralph.get_pos() - cameraNp.get_pos();
   camVec.set_z(0);
   float camDist = camVec.length();
   camVec.normalize();
   if(camDist > 10.0)
      {
      cameraNp.set_pos(cameraNp.get_pos() + camVec*(camDist-10));
      camDist = 10.0;
      }
   if(camDist < 5.0)
      {
      cameraNp.set_pos(cameraNp.get_pos() - camVec*(5-camDist));
      camDist = 5.0;
      }

   // Now check for collisions.

   NodePath renderNp = m_windowFrameworkPtr->get_render();
   m_collisionTraverser.traverse(renderNp);

   // Adjust ralph's Z coordinate.  If ralph's ray hit terrain,
   // update his Z. If it hit anything else, or didn't hit anything, put
   // him back where he was last frame.

   m_ralphGroundHandlerPtr->sort_entries();
   if(m_ralphGroundHandlerPtr->get_num_entries() > 0 &&
      m_ralphGroundHandlerPtr->get_entry(0)->get_into_node()->get_name() == "terrain")
      {
      m_ralph.set_z(m_ralphGroundHandlerPtr->get_entry(0)->get_surface_point(renderNp).get_z());
      }
   else
      {
      m_ralph.set_pos(startPos);
      }

   // Keep the camera at one foot above the terrain,
   // or two feet above ralph, whichever is greater.

   m_camGroundHandlerPtr->sort_entries();
   if(m_camGroundHandlerPtr->get_num_entries() > 0 &&
      m_camGroundHandlerPtr->get_entry(0)->get_into_node()->get_name() == "terrain")
      {
      cameraNp.set_z(m_camGroundHandlerPtr->get_entry(0)->get_surface_point(renderNp).get_z()+1.0);
      }
   if(cameraNp.get_z() < m_ralph.get_z() + 2.0)
      {
      cameraNp.set_z(m_ralph.get_z() + 2.0);
      }

   // The camera should look in ralph's direction,
   // but it should also try to stay horizontal, so look at
   // a floater which hovers above ralph's head.

   m_floaterNp.set_pos(m_ralph.get_pos());
   m_floaterNp.set_z(m_ralph.get_z() + 2.0);
   cameraNp.look_at(m_floaterNp);
   }

void World::sys_exit(const Event* eventPtr, void* dataPtr)
   {
   exit(0);
   }

template<int key, bool value>
void World::call_set_key(const Event* eventPtr, void* dataPtr)
   {
   // preconditions
   if(dataPtr == NULL)
      {
      nout << "ERROR: template<int key> void World::call_set_key(const Event* eventPtr, void* dataPtr) "
              "parameter dataPtr cannot be NULL." << endl;
      return;
      }
   if(key < 0 || key >= K_keys)
      {
      nout << "ERROR: template<int key> void World::call_set_key(const Event* eventPtr, void* dataPtr) "
              "parameter key is out of range: " << key << endl;
      return;
      }

   static_cast<World*>(dataPtr)->set_key(static_cast<Key>(key), value);
   }

AsyncTask::DoneStatus World::call_move(GenericAsyncTask* taskPtr, void* dataPtr)
   {
   // preconditions
   if(dataPtr == NULL)
      {
      nout << "ERROR: AsyncTask::DoneStatus World::call_move(GenericAsyncTask* taskPtr, void* dataPtr) parameter dataPtr cannot be NULL." << endl;
      return AsyncTask::DS_done;
      }

   World* worldPtr = static_cast<World*>(dataPtr);
   worldPtr->move();
   return AsyncTask::DS_cont;
   }
