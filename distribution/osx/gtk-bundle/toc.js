<!-- Start of script to import html into div, toggle TOC button

	// If in HelpViewer activate the TOC display button.
	if ('HelpViewer' in window && 'showTOCButton' in window.HelpViewer) {
		function toggle() {
			var e = document.getElementById('toc');
   		 	if (e.style.display == 'block' || e.style.display=='')
   		 		{
   		    	 e.style.display = 'none';
  		  		}
  		  	else 
   		 		{
  		    	  e.style.display = 'block';
  			  	}  		  	
       	}
       	
		function showTOCButton(state) {
			window.HelpViewer.showTOCButton(state, toggle, toggle);
			window.HelpViewer.setTOCButton(state);
		}
    	window.setTimeout(showTOCButton(true), 1000);
		
	}
    window.onload=function() {
    	function include(a,b){
    		var c="/^(?:file):/"
    		var d=new XMLHttpRequest; 
    		var e=0;
    		try {
    		d.open('GET',b,!0);
    		d.onreadystatechange=function(){			
    			if (4==d.readyState) a.innerHTML=d.responseText;
    			var acc = document.getElementsByClassName("accordion");
				var i;

				for (i = 0; i < acc.length; i++) {
				  acc[i].onclick = function() {
				      var panels = document.getElementsByClassName("accordion");
  					  this.classList.toggle("active");
 				      var panel = this.parentElement.nextElementSibling;			      
 				      if (panel.tagName == "DIV") {
  					 	 if (panel.style.maxHeight){
      						panel.style.maxHeight = null;
    					  } else {
      						panel.style.maxHeight = panel.scrollHeight + "px";
    					  }
    				  } 
                  }
			   }
    		}
 		  	d.send();		   	
 		   	} catch(f){};
 		}
    	
    	var c=document.getElementsByTagName('DIV');
    	var i;
    	for(i = 0; i < c.length; i++) {
				if (c[i].hasAttribute && c[i].hasAttribute('data-include')) {
					include(c[i],c[i].getAttribute('data-include'));
    			};
    	}

	}



// End -->
