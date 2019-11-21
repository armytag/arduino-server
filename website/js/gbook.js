// Set up a new XML request to get the guestbook data from the server
var gbook = new XMLHttpRequest();
// If successful, this request will send the data to be parsed and added to the page within the gbook div
gbook.onreadystatechange = function() {
    if(this.readyState == 4 && this.status == 200) {
        createGuestBook(gbook.responseText);
    }
};
// Use a GET request on gbook.txt, and send it asynchronously (don't wait for response, that will be handled by the function above
gbook.open("GET", "gbook.txt", true);
gbook.send();

// This function parses the response text into entries and then into user and message, then adds that content to the gbook div message by message
function createGuestBook(text) {
    var gbookDiv = document.getElementById('gbook-content');
    gbookDiv.innerHTML = "";
    var entries = text.split('`');

    console.log(entries);

    // Create the 'content' array, which will split the user and message of each entry into another array 
    // This will result in a two-dimensional array of the form {[user,message] , [user,message] , ... }
    var content = new Array();
    for(let entry of entries) {
        if(entry.includes('~')) { // Exclude lines that don't include the tilda delimiter, which we assume must be bad entries.
            content.push(entry.split('~'));
        }
    }

    console.log(content);

    if(content.length > 1) {
        for(let entry of content) {
            let guest = document.createElement("h3");
            let message = document.createElement("p");
            guest.innerHTML = entry[0];
            message.innerHTML = entry[1];
            // Add all of this content to the page, putting user and message into a "message" div and appending that to the existing gbook div
            let messageDiv = document.createElement("div");
            messageDiv.classList.add("gbook-entry");
            messageDiv.appendChild(guest);
            messageDiv.appendChild(message);
            gbookDiv.appendChild(messageDiv);
        }
    } else {
        gbookDiv.innerHTML = "<h4>No messages yet, but you can be the first! Just click the (+) button below.</h4>";
    }
}

// Function for toggling the entry form, called by clicking the SVG button
function toggleForm() {
    let form = document.getElementById('form-content');
    let button = document.getElementById('form-toggle-button');
    let circle = button.children[0];

    if(form.style.display == "block") {
        form.style.display = "none";
        button.setAttribute('transform', 'rotate(0)');
        circle.setAttribute('fill', '#aaa');
    } else {
        form.style.display = "block";
        button.setAttribute('transform', 'rotate(45)');
        circle.setAttribute('fill', '#e00');
    }
}

// Function that is called when the form is submitted
function submitForm() {
    let user = document.getElementById('form-user').value;
    let message = document.getElementById('form-message').value;
    // Validate the fields data that was given
    if(user.length < 1) {
        alert("Please enter a name");
        return false;
    }
    if(user.includes('~')) {
        alert("Your name cannot include a tilda (~)");
        return false;
    }
    if(user.includes('`')) {
        alert("Your name cannot include a grave (`)");
        return false;
    }
    if(message.length < 1){
        alert("Please enter a message");
        return false;
    }
    if(message.includes('`')) {
        alert("Your message cannot include a grave (`)");
        return false;
    }

    let data = '`' + user + '~' + message + '`'; // Put the information in the proper format
    console.log(data);

    let submission = new XMLHttpRequest();
    submission.open("POST", "gbook.txt", true);
    submission.send(data);

    toggleForm();
    document.getElementById('form-content').reset();
    setTimeout(function() {window.location.reload(false)}, 1000);
}
