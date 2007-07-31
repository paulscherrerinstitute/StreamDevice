if (parent.head) {
    parent.head.document.getElementById('subtitle').innerHTML=document.title;
    parent.document.title=document.title;
    document.getElementsByTagName('h1')[0].style.display="none";
}
