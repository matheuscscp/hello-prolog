loves(vincent,mia). 
loves(marcellus,mia). 

jealous(A,B):-  loves(A,C),  loves(B,C).

?- jealous(X,Y)
