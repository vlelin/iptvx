/*

   Copyright 2018   Jan Kammerath

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

*/

app.mouse = {
	init: function(){
		/* attach mouse ups */
		$(window).mousemove(function(event){
			/* only check hover when EPG is ready */
			if(app.epg.ready){
				app.mouse.checkControlHover(event.clientX,event.clientY);
				app.mouse.checkListHover(event.clientX,event.clientY);
				app.mouse.checkStreamConfigHover(event.clientX,event.clientY);
			}
		});
		$(window).mouseup(function(event){
			/* only allow elements when EPG is ready */
			if(app.epg.ready){
				/* allow mouse scroll wheel for audio track/ subtitle list */
				app.mouse.checkTrackScroll(event.clientX,event.clientY,event.button);

				/* allow mouse scroll wheel for channel list */
				app.mouse.checkChannelScroll(event.clientX,event.clientY,event.button);

				/* show EPG when click into empty space */
				if(document.elementFromPoint(event.clientX,event.clientY).tagName == "HTML"
					&& event.button == 1){
					/* left mouse click in empty space */
					app.list.toggle(true);
					app.find.toggle(true);
					app.control.toggle(true);
					app.streamconfig.toggle(true);
					app.epg.toggle();
				}

				/* adjust volume when scroll wheel is used in empty space */
				if(document.elementFromPoint(event.clientX,event.clientY).tagName == "HTML"
					&& (event.button == 4 || event.button == 5)){
					if(event.button == 4){
						/* volume up */
						app.adjustVolume(iptvx.volume+10);
					}if(event.button == 5){
						/* volume down */
						app.adjustVolume(iptvx.volume-10);
					}
				}

				/* scroll EPG with the mouse wheel */
				if(app.epg.visible == true && (event.button == 4 || event.button == 5)){
					if(event.button == 4){
						/* volume up */
						app.epg.handleKey(38);
					}if(event.button == 5){
						/* volume down */
						app.epg.handleKey(40);
					}
				}
			}
		});
	},

	/* allow scrolling tracks with the mouse wheel */
	checkTrackScroll: function(mouseX,mouseY,button){
		if(app.streamconfig.visible == true){
			/* allow scrolling and selection with 
				the wheel or 2nd button if you like */
			if(button == 4){
				/* scroll up */
				app.streamconfig.handleKey(38);
			}if(button == 5){
				/* scroll down */
				app.streamconfig.handleKey(40);
			}if(button == 2){
				/* click on middle button */
				app.streamconfig.handleKey(13);
			}

			/* check for primary (left) button clicks */
			if(button == 1){
				var elm = document.elementFromPoint(mouseX,mouseY);
				if($(elm).hasClass("configitem")){
					var trackid = $(elm).attr("id").substring(5);
					app.streamconfig.selected = parseInt(trackid);
					app.streamconfig.handleKey(13);
				}
			}
		}
	},

	/* allow scrolling channels with the mousewheel*/
	checkChannelScroll: function(mouseX,mouseY,button){
		/* get element width and height first */
		elmWidth = $("#list").outerWidth();
		elmHeight = $("#list").outerHeight();

		/* get element position */
		elmPosY = $("#list").position().top;
		elmPosX = 0;

		/* check if list needs to be enabled */
		if(app.list.visible == true){
			if(mouseX > elmPosX && mouseX < (elmPosX+elmWidth+20)
				&& mouseY > elmPosY && mouseY < (elmPosY+elmHeight)){
				/* mouse is inside perimeter of list */
				if(button == 4){
					/* scroll up */
					app.list.handleKey(38);
				}if(button == 5){
					/* scroll down */
					app.list.handleKey(40);
				}if(button == 2){
					/* select current channel */
					app.list.handleKey(13);
				}

				/* check if it is a click of the left button */
				if(button == 1){
					var elm = document.elementFromPoint(mouseX,mouseY);
					if($(elm).hasClass("channel")){
						var selchan = $(elm).attr("id").substring(11);
						app.list.selectedChannel = selchan;
						app.epg.switchChannel(app.list.selectedChannel);
						app.list.update();
					}if($(elm).parent().hasClass("channel")){
						var selchan = $(elm).parent().attr("id").substring(11);
						app.list.selectedChannel = selchan;
						app.epg.switchChannel(app.list.selectedChannel);
						app.list.update();
					}
				}
			}
		}
	},

	/* check if the mouse hovers at the bottom 
		and we might need to enbable control */
	checkControlHover: function(mouseX,mouseY){
		/* get element width and height first */
		elmWidth = $("#control").outerWidth();
		elmHeight = $("#control").outerHeight();

		/* get element position */
		elmPosX = $("#control").position().left;
		elmPosY = $(window).innerHeight()-elmHeight;

		if(app.epg.visible == false){
			if(app.control.visible == false){
				if(mouseX > elmPosX && mouseX < (elmPosX+elmWidth)
					&& mouseY > elmPosY && mouseY < (elmPosY+elmHeight)){
					/* mouse is inside perimeter of control app */
					app.control.toggle(false);
				}
			}else{
				/* fade out control when out of bounding box */
				if(mouseX < elmPosX || mouseX > (elmPosX+elmWidth)
					|| mouseY < elmPosY || mouseY > (elmPosY+elmHeight)){
					/* mouse is inside perimeter of control app */
					app.control.toggle(true);
				}
			}
		}
	},

	checkStreamConfigHover: function(mouseX,mouseY){
		/* get element width and height first */
		elmWidth = $("#streamconfig").outerWidth();
		elmHeight = $("#streamconfig").outerHeight();

		/* get element position */
		elmPosY = $("#streamconfig").position().top;
		elmPosX = $("#streamconfig").position().left;

		/* check if list needs to be enabled */
		if(app.epg.visible == false){
			if(app.streamconfig.visible == false){
				if(mouseX > (elmPosX-elmWidth)
					&& mouseY > elmPosY 
					&& mouseY < (elmPosY+elmHeight)){
					/* mouse is inside perimeter */
					app.streamconfig.toggle(false);
				}
			}else{
				/* fade out list when out of bounding box */
				if(mouseX < (elmPosX) || mouseX > (elmPosX+elmWidth+20)
					|| mouseY < elmPosY || mouseY > (elmPosY+elmHeight)){
					/* mouse is outside perimeter */
					app.streamconfig.toggle(true);
				}
			}
		}
	},

	checkListHover: function(mouseX,mouseY){
		/* get element width and height first */
		elmWidth = $("#list").outerWidth();
		elmHeight = $("#list").outerHeight();

		/* get element position */
		elmPosY = $("#list").position().top;
		elmPosX = 0;

		/* check if list needs to be enabled */
		if(app.epg.visible == false){
			if(app.list.visible == false){
				if(mouseX > elmPosX && mouseX < (elmPosX+elmWidth+20)
					&& mouseY > elmPosY && mouseY < (elmPosY+elmHeight)){
					/* mouse is inside perimeter of list */
					app.list.toggle(false);
				}
			}else{
				/* fade out list when out of bounding box */
				if(mouseX < elmPosX || mouseX > (elmPosX+elmWidth+20)
					|| mouseY < elmPosY || mouseY > (elmPosY+elmHeight)){
					/* mouse is inside perimeter of list app */
					app.list.toggle(true);
				}
			}
		}
	}
}