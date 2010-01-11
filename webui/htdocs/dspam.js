// $Id: dspam.js,v 1.02 2009/12/25 03:00:40 sbajic Exp $

// Select intermediate checkboxes on shift-click
var shifty = false;
var lastcheckboxchecked = false;
function checkboxclicked(checkbox)
{
	var num = checkbox.id.split('-')[1];

	if (lastcheckboxchecked && shifty) {
		var start = Math.min(num, lastcheckboxchecked) + 1;
		var end   = Math.max(num, lastcheckboxchecked) - 1;
		var value = checkbox.checked;
		for (i=start; i <= end; ++i) {
			document.getElementById("checkbox-"+i).checked = value;
		}
	}
	lastcheckboxchecked = num;
}

function recordshiftiness(e)
{
	if (!e) var e = window.event;
	shifty = e && ((typeof (e.shiftKey) != 'undefined' && e.shiftKey) ||
		       e.modifiers & event.SHIFT_MASK);
}

if (window.event) document.captureEvents (event.MOUSEDOWN);
document.onmousedown = recordshiftiness;
