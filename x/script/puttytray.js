/* PuTTY Tray javascript - functions
------------------------------------------------------------------*/

/* Control routines

------------------------------------------------------------------*/

function jsMode() {
	return window.location.hash == '';
}

var PuTTYTray = {

	/*
	 * Main Startup function
	 */
	start: function()
	{
		// onLoad: start UI
		Event.observe(window, 'load', function(e)
		{
			// Initialize UI elements
			PuTTYTray.uiStart();
			
			// Preload IE fix image
			if ((browser.isIE5x || browser.isIE6x) && document.images) {
				var preloadImage = new Image();
				preloadImage.src = 'g/logo.png';
			}
		}.bind(this));

		// Add event handlers
		Event.addBehavior({
			'#navigation li:click': function(e) {
				PuTTYTray.handleClick(this);
				return false;
			}
		});
	},

	/*
	 * User Interface initialization
	 */
	uiStart: function()
	{
		if (!jsMode())
			return;

		$('content_features').style.display = 'block';
		$('content_features').style.marginBottom = 0;

		var blocks = ['content_download', 'content_authors', 'content_source'];
		for (var i = 0; i < blocks.length; ++i) {
			$(blocks[i]).style.display = 'none';
			$(blocks[i]).style.marginBottom = 0;
		}

		$('content_wrap').visibleContent = 'content_features';
		$('content_wrap').animating = false;

		// Make logo draggable
		new Draggable('icon', {
			revert: function(element) {
				element.style.top = '20px';
				element.style.left = '45px';
				if (browser.isIE5x || browser.isIE6x) {
					element.timerID = setTimeout(function() {
						fixPNGs();
					}, 250);
				}
			},
			change: function() {
				if ((browser.isIE5x || browser.isIE6x) && !$(icon).ieDragFixed) {
					fixIEIcon();
				}
			}
		});
		
		// Create drop point
		Droppables.add('icondrop', {
			accept: 'dropicon',
			onHover: function(element) {
				var dropElement = $('icondrop');
				
				clearTimeout(dropElement.timerID);
				dropElement.timerID = setTimeout(function() {
					dropElement.hovering = false;
					dropElement.setStyle({ backgroundPosition: '0px 0px' });
				}, 1000);
				
				if (!dropElement.hovering) {
					dropElement.hovering = true;
					dropElement.setStyle({ backgroundPosition: '0px -85px' });
				}			
			},
			onDrop: function(element) { 
				PuTTYTray.startDownload();
			}}
		);

		// Fix PNGs
		if (browser.isIE5x || browser.isIE6x) {
			fixPNGs();
		}
	},


	handleClick: function(element)
	{
		if ($('content_wrap').animating) { return false; }
		var temp = element.id.split('_');
		var contentLayer = 'content_' + temp[1];

		// Already open?
		if ($('content_wrap').visibleContent == contentLayer) {
			new Effect.Shake($($('content_wrap').visibleContent), {
				beforeStart: function() {
					$('content_wrap').animating = true;
				},
				afterFinish: function() {
					$('content_wrap').animating = false;
				}
			});
			return false;
		}
		
		// Switch content with animation
		new Effect.BlindUp($($('content_wrap').visibleContent), {
			duration: 0.5,
			beforeStart: function() {
				$('content_wrap').animating = true;
			},
			afterFinish: function() {
				new Effect.BlindDown($(contentLayer), {
					duration: 0.5,
					afterFinish: function() {
						$('content_wrap').visibleContent = contentLayer;
						$('content_wrap').animating = false;
					}
				});
			}
		});
	},


	/*
	 * Pops a download box when icon is dragged onto the droppable
	 */
	startDownload: function()
	{
		var version = 'https://puttytray.goeswhere.com/download/v013/putty.exe';
		if ($('downloadframe')) {
			var downloadFrame = $('downloadframe');
			downloadFrame.setAttribute('src', 'g/nothing.png');
			downloadFrame.setAttribute('src', version); 
		} else {
			var downloadFrame = document.createElement('iframe');
			downloadFrame.className = 'downloadframe';
			downloadFrame.setAttribute('id', 'downloadframe'); 
			downloadFrame.setAttribute('src', version); 
			document.body.appendChild(downloadFrame);
		}
	},


	/*
	 * Draggable Icon doesn't work properly in IE, ugly 'fix' here
	 */
	fixPNGs: function()
	{
		$('icon').ieDragFixed = false;
		$('icon').style.background = 'none';
		$('icon').style.filter = "progid:DXImageTransform.Microsoft.AlphaImageLoader(src='g/logo.png', sizingMethod='scale')";
	},

	fixIEIcon: function()
	{
		$('icon').ieDragFixed = true;
		$('icon').style.filter = '';
		$('icon').style.background = 'url(g/logo.png)';
	}
}

PuTTYTray.start();
